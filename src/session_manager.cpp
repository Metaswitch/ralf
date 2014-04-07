/**
 * @file sessionmanager.cpp Ralf session manager.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <string>
#include <map>

#include "message.hpp"
#include "sessionstore.h"
#include "session_manager.hpp"
#include "log.h"
#include "peer_message_sender.hpp"
#include "sas.h"
#include "ralfsasevent.h"
#include "peer_message_sender_factory.hpp"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

void SessionManager::handle(Message* msg)
{
  SessionStore::Session* sess = NULL;

  if (msg->record_type.isInterim() || msg->record_type.isStop())
  {
    // This relates to an existing session
    sess = _store->get_session_data(msg->call_id,
                                    msg->role,
                                    msg->function,
                                    msg->trail);

    if (sess == NULL)
    {
      // No record of the session - ignore the request
      LOG_INFO("Session for %s not found in database, ignoring message", msg->call_id.c_str());
      delete msg;
      return;
    }

    if (msg->record_type.isInterim())
    {
      SAS::Event continued_rf(msg->trail, SASEvent::CONTINUED_RF_SESSION, 0);
      continued_rf.add_var_param(sess->session_id);
      SAS::report_event(continued_rf);

      sess->acct_record_number += 1;
      // Update the store with the incremented accounting record number
      bool success = _store->set_session_data(msg->call_id,
                                              msg->role,
                                              msg->function,
                                              sess,
                                              msg->trail);
      if (!success)
      {
        // Someone has written conflicting data since we read this, so start processing this message again
        return this->handle(msg);  // LCOV_EXCL_LINE - no conflicts in UT
      }
    }
    else if  (msg->record_type.isStop())
    {
      SAS::Event end_rf(msg->trail, SASEvent::END_RF_SESSION, 0);
      end_rf.add_var_param(sess->session_id);
      SAS::report_event(end_rf);

      // Delete the session from the store and cancel the timer
      _store->delete_session_data(msg->call_id,
                                  msg->role,
                                  msg->function,
                                  msg->trail);
      LOG_INFO("Received STOP for session %s, deleting session and timer using timer ID %s", msg->call_id.c_str(), sess->timer_id.c_str());

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
    if (msg->session_refresh_time == 0) {
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
  // TODO fill in the trail ID from the message.
  PeerMessageSender* pm = _factory->newSender(0); // self-deleting
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

  LOG_DEBUG("Built INTERIM request body: %s", body.c_str());

  return body;
}

void SessionManager::on_ccf_response (bool accepted, uint32_t interim_interval, std::string session_id, int rc, Message* msg)
{
  // Log this here, as it's the first time we have access to the
  // session ID for a new session
  SAS::Event new_rf(msg->trail, SASEvent::NEW_RF_SESSION, 0);
  new_rf.add_var_param(session_id);
  SAS::report_event(new_rf);

  if (accepted)
  {
    if (msg->record_type.isInterim() &&
        (!msg->timer_interim) &&
        (msg->session_refresh_time > interim_interval))

    {
      // Interim message generated by Sprout, so update a timer to generate recurring INTERIMs

      _timer_conn->send_put(msg->timer_id,
                            interim_interval,
                            msg->session_refresh_time,
                            "/call-id/"+msg->call_id+"?timer-interim=true",
                            create_opaque_data(msg),
                            msg->trail);
      SAS::Event updated_timer(msg->trail, SASEvent::INTERIM_TIMER_RENEWED, 0);
      updated_timer.add_static_param(interim_interval);
      SAS::report_event(updated_timer);
    }
    else if (msg->record_type.isStart())
    {
      // New message from Sprout - create a new timer then insert the session into the store
      std::string timer_id = "NO_TIMER";
      if (msg->session_refresh_time > interim_interval)
      {
        _timer_conn->send_post(timer_id,  // Chronos returns a timer ID which is filled in to this parameter
                               interim_interval, // interval
                               msg->session_refresh_time, // repeat-for
                               "/call-id/"+msg->call_id+"?timer-interim=true",
                               create_opaque_data(msg),
                               msg->trail);
      };
      SAS::Event new_timer(msg->trail, SASEvent::INTERIM_TIMER_CREATED, 0);
      new_timer.add_static_param(interim_interval);
      SAS::report_event(new_timer);

      LOG_INFO("Writing session to store");
      SessionStore::Session* sess = new SessionStore::Session();
      sess->session_id = session_id;
      sess->interim_interval = interim_interval;

      sess->timer_id = timer_id;
      msg->timer_id = timer_id;

      sess->ccf = msg->ccfs;
      sess->acct_record_number = msg->accounting_record_number;
      sess->session_refresh_time = msg->session_refresh_time;

      // Do this unconditionally - if it fails, this processing has already been done elsewhere
      _store->set_session_data(msg->call_id,
                               msg->role,
                               msg->function,
                               sess,
                               msg->trail);
      delete sess;
    }

  }
  else
  {
    LOG_WARNING("Session for %s received error from CDF", msg->call_id.c_str());
    if (msg->record_type.isInterim())
    {
      if (rc == 5002)
      {
        // 5002 means the CDF has no record of this session. It's pointless to send any
        // more messages - delete the session from the store.
        LOG_INFO("Session for %s received 5002 error from CDF, deleting", msg->call_id.c_str());
        _store->delete_session_data(msg->call_id,
                                    msg->role,
                                    msg->function,
                                    msg->trail);
      }
      else if (!msg->timer_interim)
      {
        // Interim failed, but the CDF probably still knows about the session,
        // so keep sending them. We don't do this for START - if a START fails we don't record the session.

        // In error conditions our CCF might not have returned an interim interval,
        // so use the one from the store.
        if (interim_interval == 0)
        {
          interim_interval = msg->interim_interval;
        }

        if (msg->session_refresh_time > interim_interval)
        {
          LOG_INFO("Received INTERIM for session %s, updating timer using timer ID %s", msg->call_id.c_str(), msg->timer_id.c_str());

          _timer_conn->send_put(msg->timer_id,
                                interim_interval,
                                msg->session_refresh_time,
                                "/call-id/"+msg->call_id+"?timer-interim=true",
                                create_opaque_data(msg),
                                msg->trail);
        }
      }
    }
  }

  // Everything is finished and we're the last holder of the Message object - delete it.
  delete msg;
}
