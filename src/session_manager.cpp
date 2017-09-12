/**
 * @file session_manager.cpp Ralf session manager.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include <map>

#include "utils.h"
#include "message.hpp"
#include "session_store.h"
#include "session_manager.hpp"
#include "log.h"
#include "peer_message_sender.hpp"
#include "sas.h"
#include "ralfsasevent.h"
#include "peer_message_sender_factory.hpp"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

// Default value for the timer_id if a post to Chronos fails
static const std::string NO_TIMER = "NO_TIMER";

void SessionManager::handle(Message* msg)
{
  SessionStore::Session* sess = NULL;

  // This flag is used to add a session from a store in one site to another
  // site that for some reason has lost it. When it's set the SessionStore will
  // add the session to the store with a CAS of 0.
  bool new_session = false;

  if (msg->record_type.isInterim() || msg->record_type.isStop())
  {
    // This relates to an existing session
    sess = _local_store->get_session_data(msg->call_id,
                                          msg->role,
                                          msg->function,
                                          msg->trail);

    if (sess == NULL)
    {
      // Try the remote stores.
      TRC_DEBUG("Session for %s not found in local store, trying remote stores",
                msg->call_id.c_str());
      new_session = true;
      std::vector<SessionStore*>::iterator it = _remote_stores.begin();

      while ((it != _remote_stores.end()) && (sess == NULL))
      {
        sess = (*it)->get_session_data(msg->call_id,
                                       msg->role,
                                       msg->function,
                                       msg->trail);
        ++it;
      }

      if (sess == NULL)
      {
        // No record of the session - ignore the request
        TRC_INFO("Session for %s not found in database, ignoring message", msg->call_id.c_str());
        delete msg; msg = NULL;
        return;
      }
    }

    // Increment the accounting record number before building new ACR.
    sess->acct_record_number += 1;

    if (msg->record_type.isInterim())
    {
      // Update the store with the incremented accounting record number.
      Store::Status rc = _local_store->set_session_data(msg->call_id,
                                                        msg->role,
                                                        msg->function,
                                                        sess,
                                                        new_session,
                                                        msg->trail);
      if (rc == Store::Status::DATA_CONTENTION)
      {
        // Someone has written conflicting data since we read this, so start processing this message again
        return this->handle(msg);  // LCOV_EXCL_LINE - no conflicts in UT
      }

      std::vector<SessionStore*>::iterator remote_store = _remote_stores.begin();

      while (remote_store != _remote_stores.end())
      {
        new_session = false;

        SessionStore::Session* remote_sess = (*remote_store)->get_session_data(msg->call_id,
                                                                               msg->role,
                                                                               msg->function,
                                                                               msg->trail);
        if (remote_sess == NULL)
        {
          remote_sess = new SessionStore::Session();
          *remote_sess = *sess;
          new_session = true;
        }
        else
        {
          remote_sess->acct_record_number += 1;
        }

        rc = (*remote_store)->set_session_data(msg->call_id,
                                               msg->role,
                                               msg->function,
                                               remote_sess,
                                               new_session,
                                               msg->trail);
        delete remote_sess; remote_sess = NULL;

        // Move onto the next store unless we've got data contention, in which
        // case we want to try this store. If a remote site is uncontactable we
        // ignore it.
        if (rc != Store::Status::DATA_CONTENTION)
        {
          ++remote_store;
        }
      }
    }
    else if  (msg->record_type.isStop())
    {
      // Delete the session from the store and cancel the timer
      Store::Status rc =_local_store->delete_session_data(msg->call_id,
                                                          msg->role,
                                                          msg->function,
                                                          sess,
                                                          msg->trail);

      if (rc == Store::Status::DATA_CONTENTION)
      {
        // Someone has written conflicting data since we read this, so start processing this message again
        return this->handle(msg);  // LCOV_EXCL_LINE - no conflicts in UT
      }

      std::vector<SessionStore*>::iterator remote_store = _remote_stores.begin();

      while (remote_store != _remote_stores.end())
      {
        rc = (*remote_store)->delete_session_data(msg->call_id,
                                                  msg->role,
                                                  msg->function,
                                                  msg->trail);

        // Move onto the next store unless we've got data contention, in which
        // case we want to try this store. If a remote site is uncontactable we
        // ignore it.
        if (rc != Store::Status::DATA_CONTENTION)
        {
          ++remote_store;
        }
      }

      TRC_INFO("Received STOP for session %s, deleting session and timer using timer ID %s", msg->call_id.c_str(), sess->timer_id.c_str());

      if (sess->timer_id != "NO_TIMER")
      {
        _timer_conn->send_delete(sess->timer_id,
                                 msg->trail);
      }
    }

    msg->accounting_record_number = sess->acct_record_number;
    msg->ccfs = sess->ccf;
    msg->session_id = sess->session_id;
    msg->timer_id = sess->timer_id;

    if (msg->session_refresh_time == 0)
    {
      // Might not be filled in on the HTTP message
      msg->session_refresh_time = sess->session_refresh_time;
    }

    msg->interim_interval = sess->interim_interval;
    delete sess; sess = NULL;
  }
  else
  {
    /* First ACR in a session: set accounting record number to 1.
     *
     * Session refresh time and CCFs on the message were filled in by the controller based on the JSON.
     *
     * Timer ID will be generated by Chronos on a POST later.
     * Interim interval and session ID will be determined by the CCF and filled in once we have that Diameter response.
     */
    msg->accounting_record_number = 1;
  };

  // go to the Diameter stack
  PeerMessageSender* pm = _factory->newSender(msg->trail); // self-deleting
  pm->send(msg, this, _dict, _diameter_stack);
}

std::string SessionManager::create_opaque_data(Message* msg)
{
  // Create the doc object so we can share the allocator during construction
  // of the child objects.  This prevents huge numbers of re-allocs.
  rapidjson::Document doc;
  doc.SetObject();

  // The IMS-Information object
  rapidjson::Value ims_info(rapidjson::kObjectType);
  ims_info.AddMember("Role-Of-Node", msg->role, doc.GetAllocator());
  ims_info.AddMember("Node-Functionality", msg->function, doc.GetAllocator());

  // The Service-Information object
  rapidjson::Value service_info(rapidjson::kObjectType);
  service_info.AddMember("IMS-Information", ims_info, doc.GetAllocator());

  // Create the top-level event object.
  rapidjson::Value event(rapidjson::kObjectType);
  event.AddMember("Service-Information", service_info, doc.GetAllocator());
  event.AddMember("Accounting-Record-Type", 3, doc.GetAllocator()); // 3 is INTERIM.

  // Finally create the document.
  doc.AddMember("event", event, doc.GetAllocator());

  // And print to a string
  rapidjson::StringBuffer s;
  rapidjson::Writer<rapidjson::StringBuffer> w(s);
  doc.Accept(w);
  std::string body = s.GetString();

  TRC_DEBUG("Built INTERIM request body: %s", body.c_str());

  return body;
}

// This function generates a SAS event based on the response from the CCF. This
// describes the *logical* impact of the event (other events cover the protocol
// flows).
//
// EVENT ACRs are explicitly *not* logged by this function. They have no impact
// beyond the current transaction and can be debugged sufficiently using the
// protocol flow.
void SessionManager::sas_log_ccf_response(bool accepted,
                                          const std::string& session_id,
                                          Message* msg)
{
  int event_id;

  // Work out what event to log.
  if (msg->record_type.isStart())
  {
    event_id = accepted ? SASEvent::NEW_RF_SESSION_OK : SASEvent::NEW_RF_SESSION_ERR;
  }
  else if (msg->record_type.isInterim())
  {
    event_id = accepted ? SASEvent::CONTINUED_RF_SESSION_OK : SASEvent::CONTINUED_RF_SESSION_ERR;
  }
  else if (msg->record_type.isStop())
  {
    event_id = accepted ? SASEvent::END_RF_SESSION_OK : SASEvent::END_RF_SESSION_ERR;
  }
  else
  {
    // No special log required for event-based billing.
    return;
  }

  SAS::Event event(msg->trail, event_id, 0);
  event.add_static_param(msg->role);
  event.add_static_param(msg->function);
  event.add_var_param(session_id);
  SAS::report_event(event);
}

void SessionManager::on_ccf_response(bool accepted,
                                     uint32_t interim_interval,
                                     std::string session_id,
                                     int rc,
                                     Message* msg)
{
  sas_log_ccf_response(accepted, session_id, msg);

  if (interim_interval == 0)
  {
    // No interim interval was set on the response. Use the interval from the store
    // if present, otherwise set it to the session refresh time
    interim_interval = (msg->interim_interval == 0) ? msg->session_refresh_time :
                                                      msg->interim_interval;
  }

  if (accepted)
  {
    if (msg->record_type.isInterim() &&
        (!msg->timer_interim) &&
        (msg->session_refresh_time > interim_interval))
    {
      // Interim message generated by Sprout, so update a timer to generate recurring INTERIMs
      std::string timer_id = msg->timer_id;

      send_chronos_update(timer_id,
                          interim_interval,
                          msg->session_refresh_time,
                          "/call-id/"+Utils::url_escape(msg->call_id)+"?timer-interim=true",
                          create_opaque_data(msg),
                          msg->trail);

      SAS::Event updated_timer(msg->trail, SASEvent::INTERIM_TIMER_RENEWED, 0);
      updated_timer.add_static_param(interim_interval);
      SAS::report_event(updated_timer);

      // Update the timer_id if it has changed
      if (timer_id != msg->timer_id)
      {
        update_timer_id(msg, timer_id);
      }
    }
    else if (msg->record_type.isStart())
    {
      // New message from Sprout - create a new timer then insert the session into the store

      // Set the timer id initially to NO_TIMER - this isn't included in the path of the POST
      std::string timer_id = NO_TIMER;
      std::map<std::string, uint32_t> tags; tags["CALL"] = 1;

      if (msg->session_refresh_time > interim_interval)
      {
         HTTPCode status = _timer_conn->send_post(timer_id,  // Chronos returns a timer ID which is filled in to this parameter
                                                  interim_interval, // interval
                                                  msg->session_refresh_time, // repeat-for
                                                  "/call-id/"+Utils::url_escape(msg->call_id)+"?timer-interim=true",
                                                  create_opaque_data(msg),
                                                  msg->trail,
                                                  tags);

         if (status == HTTP_OK)
         {
           SAS::Event new_timer(msg->trail, SASEvent::INTERIM_TIMER_CREATED, 0);
           new_timer.add_static_param(interim_interval);
           SAS::report_event(new_timer);
         }
         else
         {
           // LCOV_EXCL_START
           TRC_ERROR("Chronos POST failed");
           // LCOV_EXCL_STOP
         }
      };

      TRC_INFO("Writing session to store");
      SessionStore::Session* sess = new SessionStore::Session();
      sess->session_id = session_id;
      sess->interim_interval = interim_interval;

      sess->timer_id = timer_id;
      msg->timer_id = timer_id;

      sess->ccf = msg->ccfs;
      sess->acct_record_number = msg->accounting_record_number;
      sess->session_refresh_time = msg->session_refresh_time;

      // Do this unconditionally - if it fails, this processing has already been done elsewhere
      _local_store->set_session_data(msg->call_id,
                                     msg->role,
                                     msg->function,
                                     sess,
                                     true,
                                     msg->trail);

      for (std::vector<SessionStore*>::iterator remote_store = _remote_stores.begin();
           remote_store != _remote_stores.end();
           ++remote_store)
      {
        (*remote_store)->set_session_data(msg->call_id,
                                          msg->role,
                                          msg->function,
                                          sess,
                                          true,
                                          msg->trail);
      }

      delete sess; sess = NULL;
    }

    // Successful ACAs are an indication of healthy behaviour
    _health_checker->health_check_passed();
  }
  else
  {
    TRC_WARNING("Session for %s received error (%d) from CDF", msg->call_id.c_str(), rc);

    if (msg->record_type.isInterim())
    {
      if (rc == 5002)
      {
        // 5002 means the CDF has no record of this session. It's pointless to send any
        // more messages - delete the session from the store.
        TRC_INFO("Session for %s received 5002 error from CDF, deleting", msg->call_id.c_str());
        _local_store->delete_session_data(msg->call_id,
                                          msg->role,
                                          msg->function,
                                          msg->trail);

        for (std::vector<SessionStore*>::iterator remote_store = _remote_stores.begin();
             remote_store != _remote_stores.end();
             ++remote_store)
        {
          (*remote_store)->delete_session_data(msg->call_id,
                                               msg->role,
                                               msg->function,
                                               msg->trail);
        }
      }
      else if (!msg->timer_interim)
      {
        // Interim failed, but the CDF probably still knows about the session,
        // so keep sending them. We don't do this for START - if a START fails we don't record the session.

        if (msg->session_refresh_time > interim_interval)
        {
          TRC_INFO("Received INTERIM for session %s, updating timer using timer ID %s", msg->call_id.c_str(), msg->timer_id.c_str());

          std::string timer_id = msg->timer_id;
          send_chronos_update(timer_id,
                              interim_interval,
                              msg->session_refresh_time,
                              "/call-id/"+Utils::url_escape(msg->call_id)+"?timer-interim=true",
                              create_opaque_data(msg),
                              msg->trail);

          // Update the timer_id if it has changed
          if (timer_id != msg->timer_id)
          {
            update_timer_id(msg, timer_id);
          }
        }
      }
    }
  }

  // Everything is finished and we're the last holder of the Message object - delete it.
  delete msg; msg = NULL;
}

// Update the timer ID for the session. This is a best effect change - if there's
// contention then this update will fail
void SessionManager::update_timer_id(Message* msg, std::string timer_id)
{
  std::vector<SessionStore*> stores = {_local_store};
  stores.insert(stores.end(),
                _remote_stores.begin(),
                _remote_stores.end());

  for (std::vector<SessionStore*>::iterator store = stores.begin();
       store != stores.end();
       ++store)
  {
    SessionStore::Session* sess = (*store)->get_session_data(msg->call_id,
                                                             msg->role,
                                                             msg->function,
                                                             msg->trail);
    if (sess != NULL)
    {
      sess->timer_id = timer_id;
      msg->timer_id = timer_id;

      (*store)->set_session_data(msg->call_id,
                                 msg->role,
                                 msg->function,
                                 sess,
                                 false,
                                 msg->trail);
    }

    delete sess; sess = NULL;
  }
}

void SessionManager::send_chronos_update(std::string& timer_id,
                                         uint32_t interim_interval,
                                         uint32_t session_refresh_time,
                                         const std::string& callback_uri,
                                         const std::string& opaque_data,
                                         SAS::TrailId trail)
{
  std::map<std::string, uint32_t> tags {{"CALL", 1}};

  if (timer_id == NO_TIMER)
  {
    // LCOV_EXCL_START
    // The initial post to Chronos must have failed. Retry the post to get a new timer ID.
    _timer_conn->send_post(timer_id,
                           interim_interval,
                           session_refresh_time,
                           callback_uri,
                           opaque_data,
                           trail,
                           tags);
    // LCOV_EXCL_STOP
  }
  else
  {
    _timer_conn->send_put(timer_id,
                          interim_interval,
                          session_refresh_time,
                          callback_uri,
                          opaque_data,
                          trail,
                          tags);
  }
}
