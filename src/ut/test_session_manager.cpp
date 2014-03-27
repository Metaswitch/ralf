/**
 * @file test_sessionstore.cpp UT for Ralf session store.
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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "localstore.h"
#include "sessionstore.h"
#include "session_manager.hpp"
#include "mock_chronos_connection.h"

#include "peer_message_sender.hpp"
#include "peer_message_sender_factory.hpp"

const SAS::TrailId FAKE_TRAIL_ID = 0;

// Simulates a request to a CDF that returns successfully
class DummyPeerMessageSender : public PeerMessageSender
{
public:
  DummyPeerMessageSender(SAS::TrailId trail) : PeerMessageSender(trail) {}

  void send(Message* msg, SessionManager* sm, Rf::Dictionary* dict, Diameter::Stack* diameter_stack)
  {
    sm->on_ccf_response(true, 100, "test_session_id", 2001, msg);
    delete this;
  };
};

class DummyPeerMessageSenderFactory : public PeerMessageSenderFactory {
  PeerMessageSender* newSender(SAS::TrailId trail) {return new DummyPeerMessageSender(trail);}
};

// Simulates a request to a CDF that returns a 5001 error
class DummyErrorPeerMessageSender : public PeerMessageSender
{
public:
  DummyErrorPeerMessageSender(SAS::TrailId trail) : PeerMessageSender(trail) {}

  void send(Message* msg, SessionManager* sm, Rf::Dictionary* dict, Diameter::Stack* diameter_stack)
  {
    sm->on_ccf_response(false, 0, "test_session_id", 5001, msg);
    delete this;
  };
};

class DummyErrorPeerMessageSenderFactory : public PeerMessageSenderFactory {
  PeerMessageSender* newSender(SAS::TrailId trail) {return new DummyErrorPeerMessageSender(trail);}
};

// Simulates a request to a CDF that returns a 5002 (session unknown) error, which is handled specially
class DummyUnknownErrorPeerMessageSender : public PeerMessageSender
{
public:
  DummyUnknownErrorPeerMessageSender(SAS::TrailId trail) : PeerMessageSender(trail) {}

  void send(Message* msg, SessionManager* sm, Rf::Dictionary* dict, Diameter::Stack* diameter_stack)
  {
    sm->on_ccf_response(false, 100, "test_session_id", 5002, msg);
    delete this;
  };
};

class DummyUnknownErrorPeerMessageSenderFactory : public PeerMessageSenderFactory {
  PeerMessageSender* newSender(SAS::TrailId trail) {return new DummyUnknownErrorPeerMessageSender(trail);}
};

class SessionManagerTest : public ::testing::Test
{
  SessionManagerTest()
  {
   _dict = NULL;
   _diameter_stack = NULL;
  }

  virtual ~SessionManagerTest()
  {
  }

  Rf::Dictionary* _dict;;
  Diameter::Stack* _diameter_stack;
};

TEST_F(SessionManagerTest, SimpleTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  SessionManager* mgr = new SessionManager(store, _dict, factory, fake_chronos, _diameter_stack);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  start_msg->ccfs.push_back("10.0.0.1");
  Message* interim_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 0, FAKE_TRAIL_ID);
  Message* stop_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(4), 0, FAKE_TRAIL_ID);

  // START should put a session in the store
  mgr->handle(start_msg);

  sess = store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(1u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  // INTERIM should keep that session in the store
  mgr->handle(interim_msg);

  sess = store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  // STOP should remove the session from the store
  mgr->handle(stop_msg);

  sess = store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  delete mgr;
  delete factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, TimeUpdateTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  SessionManager* mgr = new SessionManager(store, _dict, factory, fake_chronos, _diameter_stack);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  start_msg->ccfs.push_back("10.0.0.1");
  Message* interim_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 600, FAKE_TRAIL_ID);
  Message* stop_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(4), 0, FAKE_TRAIL_ID);

  mgr->handle(start_msg);

  sess = store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(1u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  mgr->handle(interim_msg);

  // An INTERIM message which increases the session refresh interval should be accepted
  sess = store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  mgr->handle(stop_msg);

  sess = store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  delete mgr;
  delete factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, NewCallTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  SessionManager* mgr = new SessionManager(store, _dict, factory, fake_chronos, _diameter_stack);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_TWO", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  Message* stop_msg = new Message("CALL_ID_TWO", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(4), 300, FAKE_TRAIL_ID);
  Message* start_msg_2 = new Message("CALL_ID_TWO", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);

  mgr->handle(start_msg);

  sess = store->get_session_data("CALL_ID_TWO", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  delete sess;
  mgr->handle(stop_msg);
  sess = store->get_session_data("CALL_ID_TWO", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  mgr->handle(start_msg_2);

  // Re-using call-IDs should just work
  sess = store->get_session_data("CALL_ID_TWO", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(1u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  delete mgr;
  delete factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, UnknownCallTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  SessionManager* mgr = new SessionManager(store, _dict, factory, fake_chronos, _diameter_stack);
  SessionStore::Session* sess = NULL;

  Message* interim_msg = new Message("CALL_ID_THREE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  // If we receive an INTERIM for a call not in the store, we should ignore it
  mgr->handle(interim_msg);
  sess = store->get_session_data("CALL_ID_THREE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  delete mgr;
  delete factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, CDFFailureTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyErrorPeerMessageSenderFactory* factory = new DummyErrorPeerMessageSenderFactory();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  SessionManager* mgr = new SessionManager(store, _dict, factory, fake_chronos, _diameter_stack);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  Message* interim_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  // When a START message fails, we should not store the session or handle any subsequent messages
  mgr->handle(start_msg);
  sess = store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  mgr->handle(interim_msg);
  sess = store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  delete mgr;
  delete factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, CDFInterimFailureTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  SessionManager* mgr = new SessionManager(store, _dict, factory, fake_chronos, _diameter_stack);
  DummyErrorPeerMessageSenderFactory* fail_factory = new DummyErrorPeerMessageSenderFactory();
  SessionManager* fail_mgr = new SessionManager(store, _dict, fail_factory, fake_chronos, _diameter_stack);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  Message* interim_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  mgr->handle(start_msg);
  sess = store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(1u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  // When an INTERIM message fails with an error other than 5002 "Session unknown", we should still keep the session
  fail_mgr->handle(interim_msg);
  sess = store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  delete mgr;
  delete factory;
  delete fail_mgr;
  delete fail_factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, CDFInterimUnknownTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  SessionManager* mgr = new SessionManager(store, _dict, factory, fake_chronos, _diameter_stack);
  DummyUnknownErrorPeerMessageSenderFactory* fail_factory = new DummyUnknownErrorPeerMessageSenderFactory();
  SessionManager* fail_mgr = new SessionManager(store, _dict, fail_factory, fake_chronos, _diameter_stack);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  Message* interim_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  mgr->handle(start_msg);
  sess = store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(1u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  // When an INTERIM message fails with a 5002 "Session unknown" error, we should delete the session
  fail_mgr->handle(interim_msg);
  sess = store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  delete mgr;
  delete factory;
  delete fail_mgr;
  delete fail_factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}
