/**
 * @file session_store.h Definitions of interfaces for the session store.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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

  /// Class used by the SessionStore to serialize sessions from C++ objects
  /// to the JSON format used in the store, and deserialize them.
  class JsonSerializerDeserializer
  {
  public:
    /// Destructor.
    ~JsonSerializerDeserializer() {};

    /// Serialize a Session object to the format used in the store.
    ///
    /// @param data - The session to serialize
    /// @return     - The serialized form.
    std::string serialize_session(Session *data);

    /// Deserialize some data from the store into a Session object
    ///
    /// @param s - The data to deserialize.
    ///
    /// @return  - A session object, or NULL if the data could not be
    ///            deserialized (e.g. because it is corrupt).
    Session* deserialize_session(const std::string& data);
  };

  /// Constructor that creates a SessionStore.
  ///
  /// @param store              - Pointer to the underlying data store.
  SessionStore(Store *store);

  /// Destructor
  ~SessionStore();

  // Retrieve session state for a given Call-ID.
  Session* get_session_data(const std::string& call_id,
                            const role_of_node_t role,
                            const node_functionality_t function,
                            SAS::TrailId trail);

  // Save the session object back into the store (this may fail due to CAS
  // atomicity checking).
  Store::Status set_session_data(const std::string& call_id,
                                 const role_of_node_t role,
                                 const node_functionality_t function,
                                 Session* data,
                                 bool new_session,
                                 SAS::TrailId trail);

  // Delete the session object from the store safely (this may fail due to CAS
  // atomicity checking).
  Store::Status delete_session_data(const std::string& call_id,
                                    const role_of_node_t role,
                                    const node_functionality_t function,
                                    Session* data,
                                    SAS::TrailId trail);

  // Delete the session object from the store aggressively (this will never fail
  // due to CAS atomicity checking).
  Store::Status delete_session_data(const std::string& call_id,
                                    const role_of_node_t role,
                                    const node_functionality_t function,
                                    SAS::TrailId trail);

private:
  // Serialise a session to a string, ready to store in the DB.
  std::string serialize_session(Session *session);
  Session* deserialize_session(const std::string& s);

  std::string create_key(const std::string& call_id,
                         const role_of_node_t role,
                         const node_functionality_t function);

  Store* _store;

  JsonSerializerDeserializer* _serializer;
  std::vector<JsonSerializerDeserializer*> _deserializers;
};

#endif
