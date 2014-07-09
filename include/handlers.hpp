/**
* @file handlers.cpp handlers for homestead
*
* Project Clearwater - IMS in the Cloud
* Copyright (C) 2013 Metaswitch Networks Ltd
*
* This program is free software: you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation, either version 3 of the License, or (at your
* option) any later version, along with the "Special Exception" for use of
* the program along with SSL, set forth below. This program is distributed
* in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more
* details. You should have received a copy of the GNU General Public
* License along with this program. If not, see
* <http://www.gnu.org/licenses/>.
*
* The author can be reached by email at clearwater@metaswitch.com or by
* post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
*
* Special Exception
* Metaswitch Networks Ltd grants you permission to copy, modify,
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

struct BillingControllerConfig
{
  SessionManager* mgr;
};

class BillingControllerHandler : public HttpStackUtils::Handler
{
public:
  BillingControllerHandler(HttpStack::Request& req,
                           const BillingControllerConfig* cfg,
                           SAS::TrailId trail) :
    HttpStackUtils::Handler(req, trail), _sess_mgr(cfg->mgr)
  {};
  void run();
  static Message* parse_body(std::string call_id, bool timer_interim, std::string reqbody, SAS::TrailId trail);
private:
  inline std::string call_id() {return _req.file();};
  SessionManager* _sess_mgr;
};

class BillingController:
  public HttpStackUtils::SpawningController<BillingControllerHandler, BillingControllerConfig>
{
public:
  BillingController(BillingControllerConfig* cfg) :
    SpawningController<BillingControllerHandler, BillingControllerConfig>(cfg)
  {}
  virtual ~BillingController() {}

  HttpStack::SasLogger* sas_logger(HttpStack::Request& req)
  {
    // Work out whether this is a chronos transaction or not.
    if (req.param(TIMER_INTERIM_PARAM) == "true")
    {
      return &HttpStackUtils::CHRONOS_SAS_LOGGER;
    }
    else
    {
      return &HttpStack::DEFAULT_SAS_LOGGER;
    }
  }
};

#endif
