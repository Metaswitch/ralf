/**
 * @file peer_message_sender_factory.cpp
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */
#ifndef PEER_MESSAGE_SENDER_FACTORY_CPP_
#define PEER_MESSAGE_SENDER_FACTORY_CPP_

#include "peer_message_sender.hpp"

class PeerMessageSenderFactory
{
public:
  PeerMessageSenderFactory(const std::string& dest_realm) : _dest_realm(dest_realm) {};

  virtual PeerMessageSender* newSender(SAS::TrailId trail)
  {
    return new PeerMessageSender(trail, _dest_realm);
  }

private:
  const std::string _dest_realm;
};


#endif /* PEER_MESSAGE_SENDER_FACTORY_CPP_ */
