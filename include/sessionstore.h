/**
 * @file sessionstore.h Definitions of interfaces for the session store.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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

#ifndef SESSION_STORE_H__
#define SESSION_STORE_H__

#include <string>
#include <vector>

#include "store.h"
#include "message.hpp"

class SessionStore
{
public:
  
  class Session
  {
  public:
    // The DIAMETER session ID for this call.
    // e.g. 1234567890;example.com;1234567890
    std::string session_id;

    // The CCF/ECF addresses for this session in priority order.
    // e.g. 10.0.0.1
    std::vector<std::string> ccf;
    std::vector<std::string> ecf;

    // The accounting record number for the next ACR sent.
    uint32_t acct_record_number;

    // The timer ID for Chronos (if applicable)
    std::string timer_id;

    // The session refresh time for this session as specified in the SIP
    // session expiry header.
    uint32_t session_refresh_time;

    // The interim interval time for this session as specified in the Diameter Acct-Interim-Interval AVP.
    uint32_t interim_interval;

  private:
    // CAS value for this Session.  Used to guarantee consistency
    // between memcached instances.
    uint64_t _cas;

    // The SessionStore will set/read the _cas value but no-one else
    // should.
    friend class SessionStore;
  };

  SessionStore(Store *);
  ~SessionStore();

  // Retrieve session state for a given Call-ID.
  Session* get_session_data(const std::string& call_id,
                            const role_of_node_t role,
                            const node_functionality_t function);

  // Save the session object back into the store (this may fail due to CAS atomicity
  // checking)
  bool set_session_data(const std::string& call_id,
                        const role_of_node_t role,
                        const node_functionality_t function,
                        Session* data);
  bool delete_session_data(const std::string& call_id,
                           const role_of_node_t role,
                           const node_functionality_t function);

private:
  // Serialise a session to a string, ready to store in the DB.
  std::string serialize_session(Session *data);
  Session* deserialize_session(const std::string& s);

  std::string create_key(const std::string& call_id,
                         const role_of_node_t role,
                         const node_functionality_t function);

  Store* _store;
};

#endif
