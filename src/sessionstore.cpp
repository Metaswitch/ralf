/**
 * @file sessionstore.cpp Ralf session data store.
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

#include <string>
#include <sstream>

#include "sessionstore.h"
#include "message.hpp"
#include "log.h"

SessionStore::SessionStore(Store* store) : _store(store)
{
  _serializer = new BinarySerializerDeserializer();
  _deserializers.push_back(new BinarySerializerDeserializer());
}

SessionStore::~SessionStore()
{
  delete _serializer; _serializer = NULL;

  for(std::vector<SerializerDeserializer*>::iterator it = _deserializers.begin();
      it != _deserializers.end();
      ++it)
  {
    delete *it; *it = NULL;
  }
}

SessionStore::Session* SessionStore::get_session_data(const std::string& call_id,
                                                      const role_of_node_t role,
                                                      const node_functionality_t function,
                                                      SAS::TrailId trail)
{
  std::string key = create_key(call_id, role, function);
  LOG_DEBUG("Retrieving session data for %s", key.c_str());
  Session* rc = NULL;

  std::string data;
  uint64_t cas;
  Store::Status status = _store->get_data("session", key, data, cas, trail);

  if (status == Store::Status::OK && !data.empty())
  {
    rc = deserialize_session(data);
    rc->_cas = cas;
    LOG_DEBUG("Retrieved record, CAS = %ld", rc->_cas);
  }

  return rc;
}

bool SessionStore::set_session_data(const std::string& call_id,
                                    const role_of_node_t role,
                                    const node_functionality_t function,
                                    Session* session,
                                    SAS::TrailId trail)
{
  std::string key = create_key(call_id, role, function);
  LOG_DEBUG("Saving session data for %s, CAS = %ld", key.c_str(), session->_cas);

  std::string data = serialize_session(session);

  Store::Status status = _store->set_data("session",
                                          key,
                                          data,
                                          session->_cas,
                                          session->session_refresh_time,
                                          trail);
  LOG_DEBUG("Store returned %d", status);

  return (status = Store::Status::OK);
}

bool SessionStore::delete_session_data(const std::string& call_id,
                                       const role_of_node_t role,
                                       const node_functionality_t function,
                                       SAS::TrailId trail)
{
  std::string key = create_key(call_id, role, function);
  LOG_DEBUG("Deleting session data for %s", key.c_str());

  Store::Status status = _store->delete_data("session", key, trail);
  LOG_DEBUG("Store returned %d", status);

  return (status = Store::Status::OK);
}

// Serialize a session to a string that can later be loaded by deserialize_session().
std::string SessionStore::serialize_session(Session* session)
{
  return _serializer->serialize_session(session);
}

// Deserialize a previously serialized session object.
SessionStore::Session* SessionStore::deserialize_session(const std::string& data)
{
  Session* session = NULL;

  for (std::vector<SerializerDeserializer*>::iterator it = _deserializers.begin();
       it != _deserializers.end();
       ++it)
  {
    SerializerDeserializer* deserializer = *it;

    LOG_DEBUG("Try to deserialize record with '%s' deserializer",
              deserializer->name().c_str());
    session = deserializer->deserialize_session(data);

    if (session != NULL)
    {
      LOG_DEBUG("Deserialization suceeded");
      break;
    }
    else
    {
      LOG_DEBUG("Deserialization failed");
    }
  }

  return session;
}

std::string SessionStore::create_key(const std::string& call_id,
                                     const role_of_node_t role,
                                     const node_functionality_t function)
{
  return call_id + std::to_string(role) + std::to_string(function);
}


std::string SessionStore::BinarySerializerDeserializer::
  serialize_session(Session *session)
{
  std::ostringstream oss(std::ostringstream::out|std::ostringstream::binary);

  oss << session->session_id << '\0';

  int num_ccf = session->ccf.size();
  oss.write((const char*)&num_ccf, sizeof(int));

  for (std::vector<std::string>::iterator it = session->ccf.begin();
       it != session->ccf.end();
       it++)
  {
    oss << *it << '\0';
  }

  oss.write((const char*)&session->acct_record_number, sizeof(uint32_t));

  oss << session->timer_id << '\0';

  oss.write((const char*)&session->session_refresh_time, sizeof(uint32_t));

  oss.write((const char*)&session->interim_interval, sizeof(uint32_t));

  return oss.str();
}


SessionStore::Session* SessionStore::BinarySerializerDeserializer::
  deserialize_session(const std::string& data)
{
  std::istringstream iss(data, std::istringstream::in|std::istringstream::binary);
  Session* session = new Session();

  getline(iss, session->session_id, '\0');

  int num_ccf;
  iss.read((char*)&num_ccf, sizeof(int));

  for (int ii = 0; ii < num_ccf; ii++)
  {
    std::string ccf;
    getline(iss, ccf, '\0');
    session->ccf.push_back(ccf);
  }

  iss.read((char*)&session->acct_record_number, sizeof(uint32_t));

  getline(iss, session->timer_id, '\0');

  iss.read((char*)&session->session_refresh_time, sizeof(uint32_t));

  iss.read((char*)&session->interim_interval, sizeof(uint32_t));

  return session;
}


std::string SessionStore::BinarySerializerDeserializer::name()
{
  return "binary";
}
