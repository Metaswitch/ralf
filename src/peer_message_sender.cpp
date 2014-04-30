/**
 * @file peer_message_sender.cpp
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

#include <stddef.h>
#include <errno.h>
#include "log.h"
#include "peer_message_sender.hpp"
#include "ralf_transaction.hpp"
#include "rf.h"
#include "sas.h"
#include "ralfsasevent.h"

/* Creates a PeerMessageSender. The object is deleted when:
 *   - we send an ACR to a CCF that responds
 *   - we fail to send the ACR to any of the available CCFs
 *
 *   No action should be taken after any of the above happens, as the this pointer becomes invalid.
 */
PeerMessageSender::PeerMessageSender(SAS::TrailId trail, const std::string& dest_realm) :
  _which(0),
  _trail(trail),
  _dest_realm(dest_realm)
{
}

PeerMessageSender::~PeerMessageSender()
{
}

/* Sends the message to the sequence of given CCFs.
 *
 * Does not retry on errors - only on failed sends (DIAMETER_UNABLE_TO_SEND).
 */
void PeerMessageSender::send(Message* msg, SessionManager* sm, Rf::Dictionary* dict, Diameter::Stack* diameter_stack)
{
  _msg = msg;
  _ccfs = msg->ccfs;
  _sm = sm;
  _dict = dict;
  _diameter_stack = diameter_stack;
  int_send_msg();
}

/* Actually sends the message to the current active CCF.
 *
 * After sending the message, deletes this PeerMessageSender.
 */
void PeerMessageSender::int_send_msg()
{
  std::string ccf = _ccfs[_which];
  LOG_DEBUG("Sending message to %s (number %d)", ccf.c_str(), _which);

  SAS::Event msg_sent(_msg->trail, SASEvent::BILLING_REQUEST_SENT, 0);
  msg_sent.add_var_param(ccf);
  msg_sent.add_static_param(_msg->accounting_record_number);
  SAS::report_event(msg_sent);

  RalfTransaction* tsx = new RalfTransaction(_dict, this, _msg, _trail);
  Rf::AccountingRequest acr(_dict,
                            _diameter_stack,
                            _msg->session_id,
                            ccf,
                            _dest_realm,
                            _msg->accounting_record_number,
                            _msg->received_json->FindMember("event")->value);

  // Send the message to freeDiameter.  This object could get modified by a
  // callback (including being deleted) so is not safe to reference after this
  // point.
  acr.send(tsx); return;
}

/* Called when a message has been sent and a response has been received.
 *
 * If the send succeeded (as in, the message reached it's target), call back into SessionManager and self-destruct.
 *
 * If the send failed due to routing issues, either try the backup CCF or (if there isn't one), calls back into SessionManager and self-destruct.
 */
void PeerMessageSender::send_cb(int result_code,
                                int interim_interval,
                                std::string session_id)
{
  if (result_code != ER_DIAMETER_UNABLE_TO_DELIVER)
  {
    // Send succeeded, notify the SessionManager.
    _sm->on_ccf_response(result_code == ER_DIAMETER_SUCCESS, interim_interval, session_id, result_code, _msg);
    delete this; return;
  }
  else
  {
    // Send failed
    LOG_WARNING("Failed to send ACR to %s (number %d)", _ccfs[_which].c_str(), _which);
    SAS::Event cdf_failed(_msg->trail, SASEvent::BILLING_REQUEST_NOT_SENT, 0);
    cdf_failed.add_var_param(_ccfs[_which]);
    SAS::report_event(cdf_failed);

    // Do we have a backup CCF?
    _which++;
    if (_which < _ccfs.size())
    {
      SAS::Event cdf_failover(_msg->trail, SASEvent::CDF_FAILOVER, 0);
      cdf_failover.add_var_param(_ccfs[_which]);
      SAS::report_event(cdf_failover);

      // Yes we do try again.  Must be the last thing we do (as this object
      // could be modified/freed in a callback after this point).
      int_send_msg(); return;
    }
    else
    {
      // No, we've run out, fail
      LOG_ERROR("Failed to connect to all CCFs, message not sent");
      _sm->on_ccf_response(false, 0, "", result_code, _msg);
      delete this; return;
    }
  }
}
