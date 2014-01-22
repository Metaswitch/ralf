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
#include "log.h"
#include "peer_message_sender.hpp"
//#include "ralf_transaction.hpp"

PeerMessageSender::PeerMessageSender(Message* msg, SessionManager* sm)
{
  _which = PRIMARY_CCF;
  _msg = msg;
  _ccfs = msg->ccfs;
  _sm = sm;
}

void PeerMessageSender::send()
{
  std::string ccf = _ccfs[_which];

  // Check for an existing connection
  peer_hdr* peer = NULL;
  char* diam_id = strdup(ccf.c_str());
  int rc = fd_peer_getbyid(diam_id, (long unsigned int) ccf.length(), 0, &peer);
  free(diam_id);

  if (rc == 0 && peer != NULL)
  {
    // We have an existing connection.
    int_send_msg();
  }
  else
  {
    // We don't have an existing connection - try and establish one
    peer_info myinfo;
    myinfo.pi_diamid = strdup(ccf.c_str());
    myinfo.pi_diamidlen = (long unsigned int) ccf.length();
    myinfo.config.pic_lft = 120;
    myinfo.config.pic_flags.exp = PI_EXP_INACTIVE;

    LOG_DEBUG("Connecting to %s (number %d)", ccf.c_str(), _which);
    fd_peer_add(&myinfo, "PeerMessageSender::send", PeerMessageSender::fd_add_cb, this);
  }
}

void PeerMessageSender::int_send_msg()
{
  std::string ccf = _ccfs[_which];
  LOG_DEBUG("Sending message to %s (number %d)", ccf.c_str(), _which);
  //RalfTransaction* tsx = new RalfTransaction(_sm);
  //acr_builder->build(ccf, _msg)->send(tsx);
  delete this;
}

// Static callback - just calls into the relevant instance method
void PeerMessageSender::fd_add_cb(struct peer_info* peer, void* thisptr)
{
  ((PeerMessageSender*) thisptr)->fd_add_cb(peer);
}

// Called either when a connection to a peer is in OPEN state or when there is an error.
void PeerMessageSender::fd_add_cb(struct peer_info* peer)
{
  peer_hdr* hdr = NULL;
  std::string ccf = _ccfs[_which];
  int rc = fd_peer_getbyid(peer->pi_diamid, peer->pi_diamidlen, 0, &hdr);
  if ((rc == 0) && (hdr != NULL) && (fd_peer_get_state(hdr) == STATE_OPEN))
  {
    // Connection succeeded, so send our message to this CCF.
    int_send_msg();
  }
  else
  {
    // Connection failed - do we have a backup CCF?
    LOG_WARNING("Failed to connect to %s (number %d)", ccf.c_str(), _which);
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
