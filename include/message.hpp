/**
 * Copyright (C) Metaswitch Networks
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
  SAS::TrailId trail;
};

#endif
