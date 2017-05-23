/**
 * @file ralf_transaction.hpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */
#ifndef RALF_TRANSACTION_HPP_
#define RALF_TRANSACTION_HPP_

#include "diameterstack.h"
#include "message.hpp"
#include "peer_message_sender.hpp"
#include "sas.h"

class RalfTransaction: public Diameter::Transaction
{
public:
  void on_response(Diameter::Message& rsp);
  void on_timeout();
  RalfTransaction(Diameter::Dictionary* dict,
                  PeerMessageSender* peer_sender,
                  Message* msg,
                  SAS::TrailId trail) :
    Diameter::Transaction(dict, trail),
    _peer_sender(peer_sender),
    _msg(msg)
  {
  };

private:
  PeerMessageSender* _peer_sender;
  Message* _msg;
};


#endif /* RALF_TRANSACTION_HPP_ */
