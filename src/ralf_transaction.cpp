/**
 * @file ralf_transaction.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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
  std::string session_id;

  rsp.result_code(result_code);
  rsp.get_str_from_avp(_dict->SESSION_ID, session_id);
  rsp.get_i32_from_avp(_dict->ACCT_INTERIM_INTERVAL, interim_interval);

  _peer_sender->send_cb(result_code, interim_interval, session_id);
    SAS::Event succeeded(_msg->trail, SASEvent::BILLING_REQUEST_SUCCEEDED, 0);
    succeeded.add_var_param(session_id);
    SAS::report_event(succeeded);
    SAS::Event rejected(_msg->trail, SASEvent::BILLING_REQUEST_REJECTED, 0);
    rejected.add_var_param(session_id);
    SAS::report_event(rejected);
}
