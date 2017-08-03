/**
 * @file rf.h class definition wrapping Rf
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "rf.h"
#include "log.h"

namespace Rf {

Dictionary::Dictionary() :
  RF("Diameter Base Accounting"),
  TGPP("3GPP"),
  ACCOUNTING_REQUEST("Accounting-Request"),
  ACCOUNTING_RESPONSE("Accounting-Answer"),
  SERVICE_CONTEXT_ID("Service-Context-Id")
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
  TRC_DEBUG("Building an Accounting-Request");

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

  Diameter::Dictionary::AVP service_context_dict("Service-Context-Id");
  Diameter::AVP service_context_avp(service_context_dict);
  add(service_context_avp.val_str(Rf::SERVICE_CONTEXT_ID_STR));

  if (contents.GetType() != rapidjson::kObjectType)
  {
    TRC_ERROR("Cannot build ACR from JSON type %d", contents.GetType());
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
        TRC_ERROR("Invalid format (true/false) in JSON block, ignoring");
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
    catch (Diameter::Stack::Exception& e)
    {
      TRC_WARNING("AVP %s not recognised, ignoring", it->name.GetString());
    }
  }
}

AccountingRequest::~AccountingRequest()
{
}

AccountingResponse::AccountingResponse(const Dictionary* dict,
                                       Diameter::Stack* diameter_stack,
                                       const int32_t& result_code,
                                       const std::string& session_id) :
    Diameter::Message(dict, dict->ACCOUNTING_RESPONSE, diameter_stack)
{
  TRC_DEBUG("Building an Accounting-Response");
  if (result_code)
  {
    add(Diameter::AVP(dict->RESULT_CODE).val_i32(result_code));
  }

  if (session_id != "")
  {
    add_session_id(session_id);
  }
}

AccountingResponse::~AccountingResponse()
{
}

}
