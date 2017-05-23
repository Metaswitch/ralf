/**
 * @file session_manager.hpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */
#ifndef SESSION_MANAGER_HPP_
#define SESSION_MANAGER_HPP_

#include "message.hpp"
#include "session_store.h"
#include "chronosconnection.h"
#include "rf.h"
#include "health_checker.h"

class PeerMessageSenderFactory;

class SessionManager
{
public:
  SessionManager(SessionStore* local_store,
                 std::vector<SessionStore*> remote_stores,
                 Rf::Dictionary* dict,
                 PeerMessageSenderFactory* factory,
                 ChronosConnection* timer_conn,
                 Diameter::Stack* diameter_stack,
                 HealthChecker* hc): _local_store(local_store),
                                     _remote_stores(remote_stores),
                                     _timer_conn(timer_conn),
                                     _dict(dict),
                                     _factory(factory),
                                     _diameter_stack(diameter_stack),
                                     _health_checker(hc) {};
  ~SessionManager() {};
  void handle(Message* msg);
  void on_ccf_response (bool accepted, uint32_t interim_interval, std::string session_id, int rc, Message* msg);

private:
  std::string create_opaque_data(Message* msg);
  void update_timer_id(Message* msg, std::string timer_id);
  void send_chronos_update(std::string& timer_id,
                           uint32_t interim_interval,
                           uint32_t session_refresh_time,
                           const std::string& callback_uri,
                           const std::string& opaque_data,
                           SAS::TrailId trail);
  void sas_log_ccf_response(bool accepted,
                            const std::string& session_id,
                            Message* msg);

  SessionStore* _local_store;
  std::vector<SessionStore*> _remote_stores;
  ChronosConnection* _timer_conn;
  Rf::Dictionary* _dict;
  PeerMessageSenderFactory* _factory;
  Diameter::Stack* _diameter_stack;
  HealthChecker* _health_checker;
};

#endif /* SESSION_MANAGER_HPP_ */
