/**
* @file handlers.cpp handlers for homestead
*
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
*/

#include "handlers.hpp"
#include "message.hpp"
#include "log.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

void BillingTask::run()
{
  if (_req.method() != htp_method_POST)
  {
    send_http_reply(405);
    delete this;
    return;
  }

  bool timer_interim = false;
  if (_req.param(TIMER_INTERIM_PARAM) == "true")
  {
    timer_interim = true;
    SAS::Marker cid_assoc(trail(), MARKER_ID_SIP_CALL_ID, 0);
    cid_assoc.add_var_param(call_id());
    SAS::report_marker(cid_assoc);

    SAS::Event timer_pop(trail(), SASEvent::INTERIM_TIMER_POPPED, 0);
    SAS::report_event(timer_pop);
  }

  Message* msg = NULL;
  HTTPCode rc = parse_body(call_id(), timer_interim, _req.get_rx_body(), &msg, trail());

  if (rc != HTTP_OK)
  {
    SAS::Event rejected(trail(), SASEvent::REQUEST_REJECTED_INVALID_JSON, 0);
    SAS::report_event(rejected);
    send_http_reply(rc);
  }
  else
  {
    if (msg != NULL)
    {
      TRC_DEBUG("Handle the received message");

      // The session manager takes ownership of the message object and is
      // responsible for deleting it.
      _sess_mgr->handle(msg);
      msg = NULL;
    }

    // The HTTP reply won't be sent until afer we leave this function, so by
    // putting this last we ensure that the load monitor will get a sensible
    // value for the latency
    send_http_reply(rc);
  }

  delete this;
}

HTTPCode BillingTask::parse_body(std::string call_id,
                                 bool timer_interim,
                                 std::string reqbody,
                                 Message** msg,
                                 SAS::TrailId trail)
{
  rapidjson::Document* body = new rapidjson::Document();
  std::string bodys = reqbody;
  body->Parse<0>(bodys.c_str());
  std::vector<std::string> ccfs;
  uint32_t session_refresh_time = 0;
  role_of_node_t role_of_node;
  node_functionality_t node_functionality;

  // Log the body early so we still see it if we later determine it's invalid.
  if (Log::enabled(Log::DEBUG_LEVEL))
  {
    if (body->HasParseError())
    {
      // Print the body from the source string.  We can't pretty print an
      // invalid document.
      TRC_DEBUG("Handling request, Body:\n%s", reqbody.c_str());
    }
    else
    {
      rapidjson::StringBuffer s;
      rapidjson::PrettyWriter<rapidjson::StringBuffer> w(s);
      body->Accept(w);
      TRC_DEBUG("Handling request, body:\n%s", s.GetString());
    }
  }


  // Verify that the body is correct JSON with an "event" element
  if (!(*body).IsObject() ||
      !(*body).HasMember("event") ||
      !(*body)["event"].IsObject())
  {
    TRC_WARNING("JSON document was either not valid or did not have an 'event' key");
    delete body;
    return HTTP_BAD_REQUEST;
  }

  // Verify the Role-Of-Node and Node-Functionality AVPs are present (we use these
  // to distinguish devices in path for the same SIP call ID.
  if ((!(*body)["event"].HasMember("Service-Information")) ||
      (!(*body)["event"]["Service-Information"].IsObject()) ||
      (!(*body)["event"]["Service-Information"].HasMember("IMS-Information")) ||
      (!(*body)["event"]["Service-Information"]["IMS-Information"].IsObject()))
  {
    TRC_ERROR("IMS-Information not included in the event description");
    delete body;
    return HTTP_BAD_REQUEST;
  }
  else
  {
    rapidjson::Value& ims_information_json = (*body)["event"]["Service-Information"]["IMS-Information"];
    rapidjson::Value::MemberIterator role_of_node_json = ims_information_json.FindMember("Role-Of-Node");
    if ((role_of_node_json == ims_information_json.MemberEnd()) || !(role_of_node_json->value.IsInt()))
    {
      TRC_ERROR("No Role-Of-Node in IMS-Information");
      delete body;
      return HTTP_BAD_REQUEST;
    }

    role_of_node = (role_of_node_t)role_of_node_json->value.GetInt();

    rapidjson::Value::MemberIterator node_function_json = ims_information_json.FindMember("Node-Functionality");
    if ((node_function_json == ims_information_json.MemberEnd()) || !(node_function_json->value.IsInt()))
    {
      TRC_ERROR("No Node-Functionality in IMS-Information");
      delete body;
      return HTTP_BAD_REQUEST;
    }

    node_functionality = (node_functionality_t)node_function_json->value.GetInt();
  }

  // Verify that there is an Accounting-Record-Type and it is one of
  // the four valid types
  if (!((*body)["event"].HasMember("Accounting-Record-Type") &&
        ((*body)["event"]["Accounting-Record-Type"].IsInt())))
  {
    TRC_WARNING("Accounting-Record-Type not available in JSON");
    delete body;
    return HTTP_BAD_REQUEST;
  }

  Rf::AccountingRecordType record_type((*body)["event"]["Accounting-Record-Type"].GetInt());
  if (!record_type.isValid())
  {
    TRC_ERROR("Accounting-Record-Type was not one of START/INTERIM/STOP/EVENT");
    delete body;
    return HTTP_BAD_REQUEST;
  }

  // Parsed enough to SAS-log the message.
  SAS::Event incoming(trail, SASEvent::INCOMING_REQUEST, 0);
  incoming.add_static_param(record_type.code());
  incoming.add_static_param(node_functionality);
  SAS::report_event(incoming);

  // Get the Acct-Interim-Interval if present
  if ((*body)["event"].HasMember("Acct-Interim-Interval") &&
      (*body)["event"]["Acct-Interim-Interval"].IsInt())
  {
    session_refresh_time = (*body)["event"]["Acct-Interim-Interval"].GetInt();
  }

  // If we have a START or EVENT Accounting-Record-Type, we must have
  // a list of CCFs to use as peers.
  // If these are missing, Ralf can't send the ACR onto a CDF, but it has
  // successfully processed the request. Log this and return 200 OK with
  // no further processing.
  if (record_type.isStart() || record_type.isEvent())
  {
    if (!((body->HasMember("peers")) && (*body)["peers"].IsObject()))
    {
      TRC_ERROR("JSON lacked a 'peers' object (mandatory for START/EVENT)");
      SAS::Event missing_peers(trail, SASEvent::INCOMING_REQUEST_NO_PEERS, 0);
      missing_peers.add_static_param(record_type.code());
      SAS::report_event(missing_peers);

      delete body;
      return HTTP_OK;
    }

    if (!((*body)["peers"].HasMember("ccf")) ||
        !((*body)["peers"]["ccf"].IsArray()) ||
        ((*body)["peers"]["ccf"].Size() == 0))
    {
      TRC_ERROR("JSON lacked a 'ccf' array, or the array was empty (mandatory for START/EVENT)");
      delete body;
      return HTTP_BAD_REQUEST;
    }

    for (rapidjson::SizeType i = 0; i < (*body)["peers"]["ccf"].Size(); i++)
    {
      if (!(*body)["peers"]["ccf"][i].IsString())
      {
        TRC_ERROR("JSON contains a 'ccf' array but not all the elements are strings");
        delete body;
        return HTTP_BAD_REQUEST;
      }
      TRC_DEBUG("Adding CCF %s", (*body)["peers"]["ccf"][i].GetString());
      ccfs.push_back((*body)["peers"]["ccf"][i].GetString());
    }
  }

  *msg = new Message(call_id,
                     role_of_node,
                     node_functionality,
                     body,
                     record_type,
                     session_refresh_time,
                     trail,
                     timer_interim);
  if (!ccfs.empty())
  {
    (*msg)->ccfs = ccfs;
  }

  return HTTP_OK;
}
