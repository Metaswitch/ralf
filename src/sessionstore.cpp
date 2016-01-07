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
#include "json_parse_utils.h"
#include "ralfsasevent.h"

SessionStore::SessionStore(Store *store,
                           SerializerDeserializer*& serializer,
                           std::vector<SerializerDeserializer*>& deserializers) :
  _store(store),
  _serializer(serializer),
  _deserializers(deserializers)
{
  // We have taken ownership of the (de)serializers.
  serializer = NULL;
  deserializers.clear();
}

SessionStore::SessionStore(Store* store) : _store(store)
{
  _serializer = new JsonSerializerDeserializer();
  _deserializers.push_back(new JsonSerializerDeserializer());
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
  TRC_DEBUG("Retrieving session data for %s", key.c_str());
  Session* session = NULL;

  std::string data;
  uint64_t cas;
  Store::Status status = _store->get_data("session", key, data, cas, trail);

  if (status == Store::Status::OK && !data.empty())
  {
    // Retrieved the data, so deserialize it.
    TRC_DEBUG("Retrieved record, CAS = %ld", cas);
    session = deserialize_session(data);

    if (session != NULL)
    {
      session->_cas = cas;
    }
    else
    {
      // Could not deserialize the record. Treat it as not found.
      TRC_INFO("Failed to deserialize record");
      SAS::Event event(trail, SASEvent::SESSION_DESERIALIZATION_FAILED, 0);
      event.add_var_param(call_id);
      event.add_var_param(data);
      SAS::report_event(event);
    }
  }

  return session;
}

bool SessionStore::set_session_data(const std::string& call_id,
                                    const role_of_node_t role,
                                    const node_functionality_t function,
                                    Session* session,
                                    SAS::TrailId trail)
{
  std::string key = create_key(call_id, role, function);
  TRC_DEBUG("Saving session data for %s, CAS = %ld", key.c_str(), session->_cas);

  std::string data = serialize_session(session);

  Store::Status status = _store->set_data("session",
                                          key,
                                          data,
                                          session->_cas,
                                          2 * session->session_refresh_time,
                                          trail);
  TRC_DEBUG("Store returned %d", status);

  return (status == Store::Status::OK);
}

bool SessionStore::delete_session_data(const std::string& call_id,
                                       const role_of_node_t role,
                                       const node_functionality_t function,
                                       Session* session,
                                       SAS::TrailId trail)
{
  std::string key = create_key(call_id, role, function);
  TRC_DEBUG("Deleting session data for %s, CAS = %ld", key.c_str(), session->_cas);

  std::string data = serialize_session(session);

  Store::Status status = _store->set_data("session",
                                          key,
                                          data,
                                          session->_cas,
                                          0,
                                          trail);
  TRC_DEBUG("Store returned %d", status);

  return (status == Store::Status::OK);
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

    TRC_DEBUG("Try to deserialize record with '%s' deserializer",
              deserializer->name().c_str());
    session = deserializer->deserialize_session(data);

    if (session != NULL)
    {
      TRC_DEBUG("Deserialization succeeded");
      break;
    }
    else
    {
      TRC_DEBUG("Deserialization failed");
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


//
// (De)serializer for the binary SessionStore format.
//
//
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

  // Helper macro that bails out if we unexpectedly hit the end of the input
  // stream.
#define ASSERT_NOT_EOF(STREAM)                                                 \
if ((STREAM).eof())                                                            \
{                                                                              \
  TRC_INFO("Failed to deserialize binary document (hit EOF at %s:%d)",         \
           __FILE__, __LINE__);                                                \
  delete session; session = NULL;                                              \
  return NULL;                                                                 \
}

  getline(iss, session->session_id, '\0');
  ASSERT_NOT_EOF(iss);

  int num_ccf;
  iss.read((char*)&num_ccf, sizeof(int));
  ASSERT_NOT_EOF(iss);

  for (int ii = 0; ii < num_ccf; ii++)
  {
    std::string ccf;
    getline(iss, ccf, '\0');
    ASSERT_NOT_EOF(iss);
    session->ccf.push_back(ccf);
  }

  iss.read((char*)&session->acct_record_number, sizeof(uint32_t));
  ASSERT_NOT_EOF(iss);

  getline(iss, session->timer_id, '\0');
  ASSERT_NOT_EOF(iss);

  iss.read((char*)&session->session_refresh_time, sizeof(uint32_t));
  ASSERT_NOT_EOF(iss);

  iss.read((char*)&session->interim_interval, sizeof(uint32_t));
  // This could legitimately be the end of the stream.

  return session;
}


std::string SessionStore::BinarySerializerDeserializer::name()
{
  return "binary";
}


//
// (De)serializer for the JSON SessionStore format.
//

static const char* const JSON_SESSION_ID = "session_id";
static const char* const JSON_CCFS = "ccfs";
static const char* const JSON_ACCT_RECORD_NUM = "acct_record_num";
static const char* const JSON_TIMER_ID = "timer_id";
static const char* const JSON_REFRESH_TIME = "refresh_time";
static const char* const JSON_INTERIM_INTERVAL = "interim_interval";


std::string SessionStore::JsonSerializerDeserializer::
  serialize_session(Session *session)
{
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

  writer.StartObject();
  {
    writer.String(JSON_SESSION_ID); writer.String(session->session_id.c_str());

    writer.String(JSON_CCFS);
    writer.StartArray();
    {
      for (std::vector<std::string>::const_iterator ccf = session->ccf.begin();
           ccf != session->ccf.end();
           ++ccf)
      {
        writer.String(ccf->c_str());
      }
    }
    writer.EndArray();

    writer.String(JSON_ACCT_RECORD_NUM); writer.Int(session->acct_record_number);
    writer.String(JSON_TIMER_ID); writer.String(session->timer_id.c_str());
    writer.String(JSON_REFRESH_TIME); writer.Int(session->session_refresh_time);
    writer.String(JSON_INTERIM_INTERVAL); writer.Int(session->interim_interval);
  }
  writer.EndObject();

  return sb.GetString();
}


SessionStore::Session* SessionStore::JsonSerializerDeserializer::
  deserialize_session(const std::string& data)
{
  TRC_DEBUG("Deserialize JSON document: %s", data.c_str());

  rapidjson::Document doc;
  doc.Parse<0>(data.c_str());

  if (doc.HasParseError())
  {
    TRC_DEBUG("Failed to parse document");
    return NULL;
  }

  Session* session = new Session();

  try
  {
    JSON_GET_STRING_MEMBER(doc, JSON_SESSION_ID, session->session_id);

    JSON_ASSERT_CONTAINS(doc, JSON_CCFS);
    JSON_ASSERT_ARRAY(doc[JSON_CCFS]);

    for (rapidjson::Value::ConstValueIterator ccfs_it = doc[JSON_CCFS].Begin();
         ccfs_it != doc[JSON_CCFS].End();
         ++ccfs_it)
    {
      JSON_ASSERT_STRING(*ccfs_it);
      session->ccf.push_back(ccfs_it->GetString());
    }

    JSON_GET_INT_MEMBER(doc, JSON_ACCT_RECORD_NUM, session->acct_record_number);
    JSON_GET_STRING_MEMBER(doc, JSON_TIMER_ID, session->timer_id);
    JSON_GET_INT_MEMBER(doc, JSON_REFRESH_TIME, session->session_refresh_time);
    JSON_GET_INT_MEMBER(doc, JSON_INTERIM_INTERVAL, session->interim_interval);
  }
  catch(JsonFormatError err)
  {
    TRC_INFO("Failed to deserialize JSON document (hit error at %s:%d)",
             err._file, err._line);
    delete session; session = NULL;
  }

  return session;
}


std::string SessionStore::JsonSerializerDeserializer::name()
{
  return "JSON";
}
