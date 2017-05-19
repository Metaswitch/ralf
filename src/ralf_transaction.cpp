/**
 * @file ralf_transaction.cpp
 *
 * Copyright (C) Metaswitch Networks
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "ralf_transaction.hpp"
#include "ralfsasevent.h"

void RalfTransaction::on_timeout()
{
  _peer_sender->send_cb(ER_DIAMETER_UNABLE_TO_DELIVER, 0, "");
}

// Handles the Accounting-Control-Answer from the CCF, parsing out the data the SessionManager needs.
void RalfTransaction::on_response(Diameter::Message& rsp)
{
  int result_code = 0;
  int interim_interval = 0;
  std::string session_id = "<value not found in Diameter message>";

  rsp.result_code(result_code);
  rsp.get_str_from_avp(_dict->SESSION_ID, session_id);

  // This isn't a mandatory AVP. If it's missing, the interim interval is
  // set to 0.
  rsp.get_i32_from_avp(_dict->ACCT_INTERIM_INTERVAL, interim_interval);

  if (result_code == 2001)
  {
    SAS::Event succeeded(_msg->trail, SASEvent::BILLING_REQUEST_SUCCEEDED, 0);
    succeeded.add_var_param(session_id);
    SAS::report_event(succeeded);
  }
  else
  {
    SAS::Event rejected(_msg->trail, SASEvent::BILLING_REQUEST_REJECTED, 0);
    rejected.add_var_param(session_id);
    SAS::report_event(rejected);
  }

  _peer_sender->send_cb(result_code, interim_interval, session_id);
}
