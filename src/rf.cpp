/**
 * @file rf.h class definition wrapping Rf
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

#include "rf.h"
#include "log.h"

namespace Rf {

Dictionary::Dictionary() :
  RF("Diameter Base Accounting"),
  TGPP("3GPP"),
  ACCOUNTING_REQUEST("Accounting-Request"),
  ACCOUNTING_RESPONSE("Accounting-Answer")
{
}

// Create an ACR message from a JSON descriptor.  Most AVPs are auto-created from the
// contents parameter which should be a JSON object with keys named after AVPs.  For example
// this object could be the "event" part of the original HTTP request received by Ralf.
AccountingRequest::AccountingRequest(const Dictionary* dict,
                                     Diameter::Stack* diameter_stack,
                                     const std::string& session_id,
                                     const std::string& dest_host,
                                     const std::string& dest_realm,
                                     const uint32_t& record_number,
                                     const rapidjson::Value& contents) :
  Diameter::Message(dict, dict->ACCOUNTING_REQUEST, diameter_stack)
{
  LOG_DEBUG("Building an Accounting-Request");

  // Fill in the default fields
  if (session_id == "")
  {
    add_new_session_id();
  }
  else
  {
    add_session_id(session_id);
  }
  add_origin();
  add_app_id(Dictionary::Application::ACCT, dict->RF);

  // Fill in contributed fields
  Diameter::Dictionary::AVP dest_host_dict("Destination-Host");
  Diameter::AVP dest_host_avp(dest_host_dict);
  add(dest_host_avp.val_str(dest_host));

  Diameter::Dictionary::AVP dest_realm_dict("Destination-Realm");
  Diameter::AVP dest_realm_avp(dest_realm_dict);
  add(dest_realm_avp.val_str(dest_realm));

  Diameter::Dictionary::AVP record_number_dict("Accounting-Record-Number");
  Diameter::AVP record_number_avp(record_number_dict);
  add(record_number_avp.val_i32(record_number));

  if (contents.GetType() != rapidjson::kObjectType)
  {
    LOG_ERROR("Cannot build ACR from JSON type %d", contents.GetType());
    return;
  }

  // Fill in the dynamic fields
  for (rapidjson::Value::ConstMemberIterator it = contents.MemberBegin();
       it != contents.MemberEnd();
       ++it)
  {
    try
    {
      switch (it->value.GetType())
      {
      case rapidjson::kFalseType:
      case rapidjson::kTrueType:
      case rapidjson::kNullType:
        LOG_ERROR("Invalid format (true/false) in JSON block, ignoring");
        continue;
      case rapidjson::kStringType:
      case rapidjson::kNumberType:
      case rapidjson::kObjectType:
        {
          Diameter::Dictionary::AVP new_dict(VENDORS, it->name.GetString());
          Diameter::AVP avp(new_dict);
          add(avp.val_json(VENDORS, new_dict, it->value));
        }
        break;
      case rapidjson::kArrayType:
        for (rapidjson::Value::ConstValueIterator array_iter = it->value.Begin();
             array_iter !=  it->value.End();
             ++array_iter)
        {
          Diameter::Dictionary::AVP new_dict(VENDORS, it->name.GetString());
          Diameter::AVP avp(new_dict);
          add(avp.val_json(VENDORS, new_dict, *array_iter));
        }
        break;
      }
    }
    catch (Diameter::Stack::Exception e)
    {
      LOG_WARNING("AVP %s not recognised, ignoring", it->name.GetString());
    }
  }
}

AccountingRequest::~AccountingRequest()
{
}

AccountingResponse::AccountingResponse(const Dictionary* dict,
                                       Diameter::Stack* diameter_stack) :
    Diameter::Message(dict, dict->ACCOUNTING_RESPONSE, diameter_stack)
{
  LOG_DEBUG("Building an Accounting-Response");
}

AccountingResponse::~AccountingResponse()
{
}

}
