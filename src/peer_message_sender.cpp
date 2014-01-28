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

#define PRIMARY_CCF 0
#define SECONDARY_CCF 1

#include <stddef.h>
#include <errno.h>
#include "log.h"
#include "peer_message_sender.hpp"
#include "ralf_transaction.hpp"
#include "rf.h"

/* Creates a PeerMessageSender. The object is deleted when:
 *   - we call send() and the call to fd_peer_add() fails
 *   - we fail to connect to any of the available CCFs
 *   - we successfully send a message
 *
 *   No action should be taken after any of the above happens, as the this pointer becomes invalid.
 */
PeerMessageSender::PeerMessageSender(Message* msg, SessionManager* sm)
{
  _which = PRIMARY_CCF;
  _msg = msg;
  _ccfs = msg->ccfs;
  _sm = sm;
}

/* Sends the message to the primary given CCF or, if it can't connect to that CCF, to the backup CCF.
 *
 * Does not retry on errors - only on failed connections.
 *
 * If the call to fd_peer_add fails (e.g. with ENOMEM), calls back into the SessionManager then deletes this PeerMessageSender.
 *  */
void PeerMessageSender::send()
{
  std::string ccf = _ccfs[_which];

  // Check for an existing connection
  peer_hdr* peer = NULL;
  DiamId_t diam_id = strdup(ccf.c_str());
  int rc = fd_peer_getbyid(diam_id, (long unsigned int) ccf.length(), 0, &peer);
  LOG_DEBUG("Checking connection to %s", diam_id);

  if (rc == 0 && peer != NULL)
  {
    // We have an existing connection.
    int_send_msg();
  }
  else
  {
    // We don't have an existing connection - try and establish one
    peer_info myinfo;
    myinfo.pi_diamid = diam_id;
    myinfo.pi_diamidlen = (long unsigned int) ccf.length();
    myinfo.config.pic_realm = "example.com";
    myinfo.config.pic_lft = 120;
    myinfo.config.pic_flags.exp = PI_EXP_INACTIVE;
    fd_list_init(&myinfo.pi_endpoints, NULL);

    LOG_DEBUG("Connecting to %s (number %d)", myinfo.pi_diamid, _which);
    int rc = fd_peer_add(&myinfo, "PeerMessageSender::send", PeerMessageSender::fd_add_cb, this);
    if (rc == EEXIST)
    {
      // This peer has been added between checking the connection and trying to add it - send the message
      int_send_msg();
    }
    else if (rc != 0)
    {
      LOG_ERROR("fd_peer_add failed to add peer %s, rc %d", myinfo.pi_diamid, rc);
      _sm->on_ccf_response(false, 0, "", 0, _msg);
      delete this;
    }
  }
}

/* Actually sends the message to the current active CCF.
 *
 * After sending the message, deletes this PeerMessageSender.
 *  */
void PeerMessageSender::int_send_msg()
{
  std::string ccf = _ccfs[_which];
  LOG_DEBUG("Sending message to %s (number %d)", ccf.c_str(), _which);
  Rf::Dictionary dict;
  RalfTransaction* tsx = new RalfTransaction(&dict, _sm, _msg);
  Rf::AccountingChargingRequest acr(&dict, ccf, _msg->accounting_record_number, _msg->received_json->FindMember("event")->value);
  acr.send(tsx);
  delete this;
}

// Static callback - just calls into the relevant instance method
void PeerMessageSender::fd_add_cb(struct peer_info* peer, void* thisptr)
{
  ((PeerMessageSender*) thisptr)->fd_add_cb(peer);
}

/* Called either when a connection to a peer is in OPEN state or when there is an error.
 *
 * If the connection succeeded, calls int_send_msg() which then deletes this PeerMessageSender.
 *
 * If the connection failed, either try the backup CCF or (if there isn't one), calls back into SessionManager then deletes this PeerMessageSender.
 */
void PeerMessageSender::fd_add_cb(struct peer_info* peer)
{
  peer_hdr* hdr = NULL;
  int rc = fd_peer_getbyid(peer->pi_diamid, peer->pi_diamidlen, 0, &hdr);
  if ((rc == 0) && (hdr != NULL) && (fd_peer_get_state(hdr) == STATE_OPEN))
  {
    // Connection succeeded, so send our message to this CCF.
    int_send_msg();
  }
  else
  {
    // Connection failed - do we have a backup CCF?
    LOG_WARNING("Failed to connect to %s (number %d)", _ccfs[_which].c_str(), _which);
    if (_which == PRIMARY_CCF && (_ccfs.size() > 1))
    {
      // Yes we do - select it and try again.
      _which = SECONDARY_CCF;
      send();
    }
    else
    {
      // No, or we've already used it - fail and log.
      LOG_ERROR("Failed to connect to all CCFs, message not sent");
      _sm->on_ccf_response(false, 0, "", 0, _msg);
      delete this;
    }
  }
}
