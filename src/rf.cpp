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

namespace Rf {

Dictionary::Dictionary() :
  ACCOUNTING_CHARGING_REQUEST("Accounting-Charging-Request"),
  ACCOUNTING_CHARGING_RESPONSE("Accounting-Charging-Response")
{
}

AccountingChargingRequest::AccountingChargingRequest(const Dictionary* dict,
                                                     const std::string& dest_host,
                                                     const uint32_t& record_number,
                                                     const rapidjson::Value& contents) :
                                                     Diameter::Message(dict,
                                                                       dict->ACCOUNTING_CHARGING_REQUEST)
{
  LOG_DEBUG("Building an Accounting-Charging=Request");

  // Fill in the default fields
  add_new_session_id();
  add_origin();

  // Fill in contributed fields
  add(Diameter::AVP("Destination-Host").val_str(dest_host));
  add(Diameter::AVP("Accounting-Record-Number").val_i32(record_number));

  // Fill in the dynamic fields
  for (rapidjson::Value::ConstMemberIterator it = contents.MemberBegin();
       it != contents.MemberEnd();
       ++it)
  {
    switch (it->value.GetType())
    {
    case rapidjson::kFalseType:
    case rapidjson::kTrueType:
    case rapidjson::kNullType:
      LOG_ERROR("Invalid format (true/false) in JSON block, ignoring");
      continue;
    case rapidjson::kStringType:
      add(Diameter::AVP(it->name.GetString()).val_str(it->value.GetString()));
      break;
    case rapidjson::kNumberType:
      add(Diameter::AVP(it->name.GetString()).val_u32(it->value.GetUint()));
      break;
    case rapidjson::kArrayType:
      for (rapidjson::Value::ConstValueIterator ary_it = it->value.Begin();
           ary_it != it->value.End();
           ary_it++)
      {
        switch (ary_it->GetType())
        {
        case rapidjson::kNullType:
        case rapidjson::kTrueType:
        case rapidjson::kFalseType:
        case rapidjson::kObjectType:
        case rapidjson::kArrayType:
          LOG_ERROR("Invalid format for body of repeated AVP, ignoring");
          continue;
        case rapidjson::kNumberType:
          add(Diameter::AVP(it->name.GetString()).val_u32(ary_it->GetUint()));
          break;
        case rapidjson::kStringType:
          add(Diameter::AVP(it->name.GetString()).val_str(ary_it->GetString()));
          break;
        }
      }
      break;
    case rapidjson::kObjectType:
      add(Diameter::AVP(it->name.GetString()).val_json(it->value));
      break;
    }
  }   
}

AccountingChargingRequest::~AccountingChargingRequest()
{
}

AccountingChargingResponse::AccountingChargingResponse(const Dictionary* dict) :
                                                       Diameter::Message(dict, dict->ACCOUNTING_CHARGING_RESPONSE)
{
}

AccountingChargingResponse::~AccountingChargingResponse()
{
}

}
