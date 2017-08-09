/**
 * @file rf.h Class definition wrapping Rf
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef RF_H__
#define RF_H__

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <rapidjson/document.h>
#include <string>

#include "diameterstack.h"
#include "log.h"

namespace Rf {

const std::vector<std::string> VENDORS { "3GPP", "" };

// The service context id will need updating when the version of Spec TS32.299
// that we support changes - currently v10 is supported. The current format
// follows what is specified in Chapter 7.1.12. No operator-specific extensions
// are required, and a full stop is not present in before "MNC" for
// consistency with other products.
static const std::string SERVICE_CONTEXT_ID_STR = "MNC.MCC.10.32260@3gpp.org";

class AccountingRecordType {
public:
  AccountingRecordType(int type): _type(type) {};
  bool isValid() {return ((_type <= 4) && (_type >= 1));};
  bool isEvent() {return (_type == 1);}
  bool isStart() {return (_type == 2);}
  bool isInterim() {return (_type == 3);}
  bool isStop() {return (_type == 4);}
  uint32_t code() {return _type;}

private:
  uint32_t _type;
};

class Dictionary : public Diameter::Dictionary
{
public:
  Dictionary();
  const Diameter::Dictionary::Application RF;
  const Diameter::Dictionary::Vendor TGPP;
  const Diameter::Dictionary::Message ACCOUNTING_REQUEST;
  const Diameter::Dictionary::Message ACCOUNTING_RESPONSE;
  const Diameter::Dictionary::AVP SERVICE_CONTEXT_ID;
};

class AccountingRequest : public Diameter::Message
{
public:
  AccountingRequest(const Dictionary* dict,
                    Diameter::Stack* diameter_stack,
                    const std::string& session_id,
                    const std::string& dest_host,
                    const std::string& dest_realm,
                    const uint32_t& record_number,
                    const rapidjson::Value& contents);
  inline AccountingRequest(Diameter::Message& msg) : Diameter::Message(msg) {};
  ~AccountingRequest();
};

class AccountingResponse : public Diameter::Message
{
public:
  AccountingResponse(const Dictionary* dict,
                     Diameter::Stack* diameter_stack,
                     const int32_t& result_code,
                     const std::string& session_id);
  inline AccountingResponse(Diameter::Message& msg) : Diameter::Message(msg) {};
  ~AccountingResponse();
};

}

#endif
