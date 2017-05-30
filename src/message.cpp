/**
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
*/

#include <string>
#include "rapidjson/document.h"
#include "message.hpp"

/* Constructor of Message. Takes ownership of the passed-in
   rapidjson::Document pointer. */
Message::Message(const std::string& call_id,
                 role_of_node_t role,
                 node_functionality_t function,
                 rapidjson::Document* body,
                 Rf::AccountingRecordType record_type,
                 uint32_t session_refresh_time,
                 SAS::TrailId trail,
                 bool timer_interim):
  call_id(call_id),
  role(role),
  function(function),
  received_json(body),
  record_type(record_type),
  timer_interim(timer_interim),
  interim_interval(0),
  session_refresh_time(session_refresh_time),
  trail(trail)
{};

/* Deletes the enclosed rapidjson::Document. */
Message::~Message()
{
    delete this->received_json;
}
