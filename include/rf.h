/**
 * @file rf.h Class definition wrapping Rf
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

#ifndef RF_H__
#define RF_H__

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <rapidjson/document.h>
#include <string>

#include "diameterstack.h"
#include "log.h"

namespace Rf {

class AccountingRecordType {
public:
  AccountingRecordType(int type): _type(type) {};
  bool isValid() {return ((_type <= 4) && (_type >= 1));};
  bool isEvent() {return (_type == 1);}
  bool isStart() {return (_type == 2);}
  bool isInterim() {return (_type == 3);}
  bool isStop() {return (_type == 4);}

private:
  uint32_t _type;
};

class Dictionary : public Diameter::Dictionary
{
public:
  Dictionary();
  const Diameter::Dictionary::Application RF;
  const Diameter::Dictionary::Message ACCOUNTING_REQUEST;
  const Diameter::Dictionary::Message ACCOUNTING_RESPONSE;
};

class AccountingRequest : public Diameter::Message
{
public:
  AccountingRequest(const Dictionary* dict,
                    const std::string& dest_host,
                    const uint32_t& record_number,
                    const rapidjson::Value& contents);
  ~AccountingRequest();
};

class AccountingResponse : public Diameter::Message
{
public:
  AccountingResponse(const Dictionary* dict);
  ~AccountingResponse();
};

}

#endif
