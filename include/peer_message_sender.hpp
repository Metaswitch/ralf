/**
 * @file peer_message_sender.hpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */
#ifndef PEER_MESSAGE_SENDER_HPP_
#define PEER_MESSAGE_SENDER_HPP_

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>

#include "rf.h"
#include "message.hpp"
#include "session_manager.hpp"

// A PeerMessageSender is responsible for ensuring that a connection is open
// to either the primary or backup CCF, and once a connection has been opened,
// sending the message to it.
class PeerMessageSender
{
public:
  PeerMessageSender(SAS::TrailId trail, const std::string& dest_realm);
  virtual ~PeerMessageSender();
  virtual void send(Message* msg,
                    SessionManager* sm,
                    Rf::Dictionary* dict,
                    Diameter::Stack* diameter_stack);

  void send_cb(int result_cdoe, int interim_interval, std::string session_id);

private:
  void int_send_msg();

  Message* _msg;
  unsigned int _which;
  std::vector<std::string> _ccfs;
  SessionManager* _sm;
  Rf::Dictionary* _dict;
  Diameter::Stack* _diameter_stack;
  SAS::TrailId _trail;
  const std::string _dest_realm;
};

#endif /* PEER_MESSAGE_SENDER_HPP_ */
