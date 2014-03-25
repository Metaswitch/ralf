/**
* Project Clearwater - IMS in the Cloud
* Copyright (C) 2014 Metaswitch Networks Ltd
*
* This program is free software: you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation, either version 3 of the License, or (at your
* option) any later version, along with the "Special Exception" for use of
* the program along with SSL, set forth below. This program is distributed
* in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more
* details. You should have received a copy of the GNU General Public
* License along with this program. If not, see
* <http://www.gnu.org/licenses/>.
*
* The author can be reached by email at clearwater@metaswitch.com or by
* post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
*
* Special Exception
* Metaswitch Networks Ltd grants you permission to copy, modify,
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

#ifndef RALF_MESSAGE_HPP
#define RALF_MESSAGE_HPP

#include <vector>
#include <string>
#include "rapidjson/document.h"
#include "rf.h"
#include "sas.h"

enum role_of_node_t
{
  ORIGINATING=0,
  TERMINATING=1
};

enum node_functionality_t
{
  SCSCF = 0,
  PCSCF = 1,
  ICSCF = 2,
  MRFC = 3,
  MGCF = 4,
  BGCF = 5,
  AS = 6,
  IBCF = 7,
  SGW = 8,
  PGW = 9,
  HSGW = 10,
  ECSCF = 11,
  MME = 12,
  TRF = 13,
  TF = 14,
  ATCF = 15
};

struct Message
{
  Message(const std::string& call_id,
          role_of_node_t role,
          node_functionality_t function,
          rapidjson::Document* body,
          Rf::AccountingRecordType record_type,
          uint32_t session_refresh_time,
          SAS::TrailId trail,
          bool timer_interim=false);
  ~Message();

  /* The identifiers (Call-Id, role and function) and the JSON
     document are known by the controller when this message is
     constructed, so are set in the constructor and shouldn't
     be modified thereafter. */
  std::string call_id;
  role_of_node_t role;
  node_functionality_t function;
  rapidjson::Document* received_json;
  Rf::AccountingRecordType record_type;
  bool timer_interim;

  /* The CCFs and ECFs may come from the controller (on initial
     messages) or from the database store (on subsequent ones). */
  std::vector<std::string> ccfs;
  std::vector<std::string> ecfs;

  /* Session ID and accounting record number are always filled in by
     the session manager. */
  std::string session_id;
  uint32_t accounting_record_number;
  std::string timer_id;

  uint32_t interim_interval;
  uint32_t session_refresh_time;
  SAS::TrailId _trail;
};

#endif
