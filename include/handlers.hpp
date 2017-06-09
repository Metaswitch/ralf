/**
* @file handlers.cpp handlers for homestead
*
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
*/

#ifndef HANDLERS_H__
#define HANDLERS_H__

#include <boost/bind.hpp>

#include "httpstack.h"
#include "httpstack_utils.h"
#include "message.hpp"
#include "session_manager.hpp"
#include "sas.h"
#include "ralfsasevent.h"

const std::string TIMER_INTERIM_PARAM = "timer-interim";

struct BillingHandlerConfig
{
  SessionManager* mgr;
};

class BillingTask : public HttpStackUtils::Task
{
public:
  BillingTask(HttpStack::Request& req,
                     const BillingHandlerConfig* cfg,
                     SAS::TrailId trail) :
    HttpStackUtils::Task(req, trail), _sess_mgr(cfg->mgr)
  {};
  void run();
  static HTTPCode parse_body(std::string call_id,
                             bool timer_interim,
                             std::string reqbody,
                             Message** msg,
                             SAS::TrailId trail);
private:
  inline std::string call_id() {return _req.file();};
  SessionManager* _sess_mgr;
};

class BillingHandler:
  public HttpStackUtils::SpawningHandler<BillingTask, BillingHandlerConfig>
{
public:
  BillingHandler(BillingHandlerConfig* cfg, bool http_acr_logging) :
    SpawningHandler<BillingTask, BillingHandlerConfig>(cfg),
    _http_acr_logging(http_acr_logging)
  {}
  virtual ~BillingHandler() {}

  HttpStack::SasLogger* sas_logger(HttpStack::Request& req)
  {
    // Work out whether this is a chronos transaction or not.
    if (req.param(TIMER_INTERIM_PARAM) == "true")
    {
      return &HttpStackUtils::CHRONOS_SAS_LOGGER;
    }
    else
    {
      if (_http_acr_logging)
      {
        // Include bodies in ACR HTTP messages logged to SAS.
        return &HttpStack::DEFAULT_SAS_LOGGER;
      }
      else
      {
        // Ommit bodies from ACR HTTP messages logged to SAS.
        return &HttpStack::PRIVATE_SAS_LOGGER;
      }
    }
  }

private:
  bool _http_acr_logging;
};

#endif
