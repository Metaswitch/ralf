/**
 * @file test_session_manager.cpp UT for Ralfs session manager.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "localstore.h"
#include "session_store.h"
#include "session_manager.hpp"
#include "mock_chronos_connection.h"
#include "mock_health_checker.hpp"

#include "peer_message_sender.hpp"
#include "peer_message_sender_factory.hpp"

const SAS::TrailId FAKE_TRAIL_ID = 0;
const std::string BILLING_REALM = "billing.example.com";

// Simulates a request to a CDF that returns successfully
class DummyPeerMessageSender : public PeerMessageSender
{
public:
  DummyPeerMessageSender(SAS::TrailId trail, const std::string& dest_realm) : PeerMessageSender(trail, dest_realm) {}

  void send(Message* msg, SessionManager* sm, Rf::Dictionary* dict, Diameter::Stack* diameter_stack)
  {
    sm->on_ccf_response(true, 100, "test_session_id", 2001, msg);
    delete this;
  };
};

class DummyPeerMessageSenderFactory : public PeerMessageSenderFactory
{
public:
  DummyPeerMessageSenderFactory(const std::string& dest_realm) : PeerMessageSenderFactory(dest_realm) {}
  virtual ~DummyPeerMessageSenderFactory(){}

  PeerMessageSender* newSender(SAS::TrailId trail) {return new DummyPeerMessageSender(trail, BILLING_REALM);}
};

// Simulates a request to a CDF that returns a 5001 error
class DummyErrorPeerMessageSender : public PeerMessageSender
{
public:
  DummyErrorPeerMessageSender(SAS::TrailId trail, const std::string& dest_realm) : PeerMessageSender(trail, dest_realm) {}

  void send(Message* msg, SessionManager* sm, Rf::Dictionary* dict, Diameter::Stack* diameter_stack)
  {
    sm->on_ccf_response(false, 0, "test_session_id", 5001, msg);
    delete this;
  };
};

class DummyErrorPeerMessageSenderFactory : public PeerMessageSenderFactory
{
public:
  DummyErrorPeerMessageSenderFactory(const std::string& dest_realm) : PeerMessageSenderFactory(dest_realm) {}
  virtual ~DummyErrorPeerMessageSenderFactory(){}

  PeerMessageSender* newSender(SAS::TrailId trail) {return new DummyErrorPeerMessageSender(trail, BILLING_REALM);}
};

// Simulates a request to a CDF that returns a 5002 (session unknown) error, which is handled specially
class DummyUnknownErrorPeerMessageSender : public PeerMessageSender
{
public:
  DummyUnknownErrorPeerMessageSender(SAS::TrailId trail, const std::string& dest_realm) : PeerMessageSender(trail, dest_realm) {}

  void send(Message* msg, SessionManager* sm, Rf::Dictionary* dict, Diameter::Stack* diameter_stack)
  {
    sm->on_ccf_response(false, 100, "test_session_id", 5002, msg);
    delete this;
  };
};

class DummyUnknownErrorPeerMessageSenderFactory : public PeerMessageSenderFactory
{
public:
  DummyUnknownErrorPeerMessageSenderFactory(const std::string& dest_realm) : PeerMessageSenderFactory(dest_realm) {}
  virtual ~DummyUnknownErrorPeerMessageSenderFactory(){}

  PeerMessageSender* newSender(SAS::TrailId trail) {return new DummyUnknownErrorPeerMessageSender(trail, BILLING_REALM);}
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
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  HealthChecker* hc = new HealthChecker();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
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
  delete hc;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, TimerIDTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  HealthChecker* hc = new HealthChecker();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
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

  // Change the stored timer - this means that the chronos PUT will return a
  // clashing timer, triggering the session manager to update the stored timer ID
  sess->timer_id = "NEW_TIMER";
  store->set_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, sess, false, FAKE_TRAIL_ID);

  delete sess;
  sess = NULL;

  // INTERIM should keep that session in the store
  mgr->handle(interim_msg);

  sess = store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);

  // The timer id should have been updated to match the id returned from the PUT.
  EXPECT_EQ("TIMER_ID", sess->timer_id);

  delete sess;
  sess = NULL;

  // STOP should remove the session from the store
  mgr->handle(stop_msg);

  sess = store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  delete mgr;
  delete hc;
  delete factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, TimeUpdateTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  HealthChecker* hc = new HealthChecker();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
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
  delete hc;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, NewCallTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  HealthChecker* hc = new HealthChecker();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
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
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  ASSERT_EQ(1u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  delete mgr;
  delete factory;
  delete hc;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, UnknownCallTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  HealthChecker* hc = new HealthChecker();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
  SessionStore::Session* sess = NULL;

  Message* interim_msg = new Message("CALL_ID_THREE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  // If we receive an INTERIM for a call not in the store, we should ignore it
  mgr->handle(interim_msg);
  sess = store->get_session_data("CALL_ID_THREE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  delete mgr;
  delete factory;
  delete hc;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, CDFFailureTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyErrorPeerMessageSenderFactory* factory = new DummyErrorPeerMessageSenderFactory(BILLING_REALM);
  HealthChecker* hc = new HealthChecker();
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
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
  delete hc;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, CDFInterimFailureTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  HealthChecker* hc = new HealthChecker();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
  DummyErrorPeerMessageSenderFactory* fail_factory = new DummyErrorPeerMessageSenderFactory(BILLING_REALM);
  SessionManager* fail_mgr = new SessionManager(store, {}, _dict, fail_factory, fake_chronos, _diameter_stack, hc);
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
  delete hc;
  delete fail_factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, CDFInterimFailureWithTimerIdChangeTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  HealthChecker* hc = new HealthChecker();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
  DummyErrorPeerMessageSenderFactory* fail_factory = new DummyErrorPeerMessageSenderFactory(BILLING_REALM);
  SessionManager* fail_mgr = new SessionManager(store, {}, _dict, fail_factory, fake_chronos, _diameter_stack, hc);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  Message* interim_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  mgr->handle(start_msg);
  sess = store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);

  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(1u, sess->acct_record_number);

  // Change the stored timer - this means that the chronos PUT will return a
  // clashing timer, triggering the session manager to update the stored timer ID
  sess->timer_id = "NEW_TIMER";
  store->set_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, sess, false, FAKE_TRAIL_ID);

  delete sess;
  sess = NULL;

  // When an INTERIM message fails with an error other than 5002 "Session unknown", we should still keep the session
  fail_mgr->handle(interim_msg);
  sess = store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);

  // The timer id should have been updated to match the id returned from the PUT.
  EXPECT_EQ("TIMER_ID", sess->timer_id);

  delete sess;
  sess = NULL;

  delete mgr;
  delete factory;
  delete fail_mgr;
  delete hc;
  delete fail_factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, CDFInterimUnknownTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  HealthChecker* hc = new HealthChecker();
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
  DummyUnknownErrorPeerMessageSenderFactory* fail_factory = new DummyUnknownErrorPeerMessageSenderFactory(BILLING_REALM);
  SessionManager* fail_mgr = new SessionManager(store, {}, _dict, fail_factory, fake_chronos, _diameter_stack, hc);
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
  delete hc;
  delete fail_factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, HealthCheckTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  MockHealthChecker* hc = new MockHealthChecker();
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  Message* interim_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  // When the CDF accepts a message, we should treat that as
  // healthy behaviour
  EXPECT_CALL(*hc, health_check_passed()).Times(1);
  mgr->handle(start_msg);

  // When the CDF accepts a message, we should treat that as
  // healthy behaviour
  EXPECT_CALL(*hc, health_check_passed()).Times(1);
  mgr->handle(interim_msg);

  delete sess;
  sess = NULL;

  delete mgr;
  delete hc;
  delete factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, HealthCheckFailureTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  MockHealthChecker* hc = new MockHealthChecker();
  DummyErrorPeerMessageSenderFactory* fail_factory = new DummyErrorPeerMessageSenderFactory(BILLING_REALM);
  SessionManager* fail_mgr = new SessionManager(store, {}, _dict, fail_factory, fake_chronos, _diameter_stack, hc);
  SessionStore::Session* sess = NULL;

  Message* interim_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  // When the CDF rejects a message, we should not treat that as
  // healthy behaviour
  EXPECT_CALL(*hc, health_check_passed()).Times(0);
  fail_mgr->handle(interim_msg);

  delete sess;
  sess = NULL;

  delete fail_mgr;
  delete hc;
  delete fail_factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, CorrectTagForwarded)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  MockChronosConnection* fake_chronos = new MockChronosConnection("http://localhost:1234");
  fake_chronos->accept_all_requests();
  MockHealthChecker* hc = new MockHealthChecker();
  DummyPeerMessageSenderFactory* factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
  SessionManager* mgr = new SessionManager(store, {}, _dict, factory, fake_chronos, _diameter_stack, hc);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);

  // When the CDF accepts a message, we should treat that as
  // healthy behaviour
  std::map<std::string, uint32_t> tags; tags["CALL"] = 1;
  EXPECT_CALL(*fake_chronos, send_post(_, _, _, _, _, _, tags)).Times(1);
  EXPECT_CALL(*hc, health_check_passed()).Times(1);
  mgr->handle(start_msg);

  delete sess;
  sess = NULL;

  delete mgr;
  delete hc;
  delete factory;
  delete fake_chronos;
  delete store;
  delete memstore;
}

class SessionManagerGRTest : public ::testing::Test
{
  SessionManagerGRTest()
  {
  }

  virtual ~SessionManagerGRTest()
  {
  }

  virtual void SetUp()
  {
    _dict = NULL;
    _diameter_stack = NULL;
    _local_memstore = new LocalStore();
    _remote_memstore1 = new LocalStore();
    _remote_memstore2 = new LocalStore();
    _local_store = new SessionStore(_local_memstore);
    _remote_store1 = new SessionStore(_remote_memstore1);
    _remote_store2 = new SessionStore(_remote_memstore2);
    _factory = new DummyPeerMessageSenderFactory(BILLING_REALM);
    _fake_chronos = new MockChronosConnection("http://localhost:1234");
    _fake_chronos->accept_all_requests();
    _hc = new HealthChecker();
    _mgr = new SessionManager(_local_store,
                              {_remote_store1, _remote_store2},
                              _dict,
                              _factory,
                              _fake_chronos,
                              _diameter_stack,
                              _hc);
  }

  virtual void TearDown()
  {
    delete _mgr; _mgr = NULL;
    delete _factory; _factory = NULL;
    delete _hc; _hc = NULL;
    delete _fake_chronos; _fake_chronos = NULL;
    delete _local_store; _local_store = NULL;
    delete _remote_store1; _remote_store1 = NULL;
    delete _remote_store2; _remote_store2 = NULL;
    delete _local_memstore; _local_memstore = NULL;
    delete _remote_memstore1; _remote_memstore1 = NULL;
    delete _remote_memstore2; _remote_memstore2 = NULL;
  }

  static Rf::Dictionary* _dict;;
  static Diameter::Stack* _diameter_stack;
  static LocalStore* _local_memstore;
  static LocalStore* _remote_memstore1;
  static LocalStore* _remote_memstore2;
  static SessionStore* _local_store;
  static SessionStore* _remote_store1;
  static SessionStore* _remote_store2;
  static DummyPeerMessageSenderFactory* _factory;
  static MockChronosConnection* _fake_chronos;
  static HealthChecker* _hc;
  static SessionManager* _mgr;
};

Rf::Dictionary* SessionManagerGRTest::_dict;
Diameter::Stack* SessionManagerGRTest::_diameter_stack;
LocalStore* SessionManagerGRTest::_local_memstore;
LocalStore* SessionManagerGRTest::_remote_memstore1;
LocalStore* SessionManagerGRTest::_remote_memstore2;
SessionStore* SessionManagerGRTest::_local_store;
SessionStore* SessionManagerGRTest::_remote_store1;
SessionStore* SessionManagerGRTest::_remote_store2;
DummyPeerMessageSenderFactory* SessionManagerGRTest::_factory;
MockChronosConnection* SessionManagerGRTest::_fake_chronos;
HealthChecker* SessionManagerGRTest::_hc;
SessionManager* SessionManagerGRTest::_mgr;

TEST_F(SessionManagerGRTest, SimpleTest)
{
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  start_msg->ccfs.push_back("10.0.0.1");
  Message* interim_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 0, FAKE_TRAIL_ID);
  Message* stop_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(4), 0, FAKE_TRAIL_ID);

  // START should put a session in all the stores.
  _mgr->handle(start_msg);

  sess = _local_store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(1u, sess->acct_record_number);
  delete sess; sess = NULL;

  sess = _remote_store1->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(1u, sess->acct_record_number);
  delete sess; sess = NULL;

  sess = _remote_store2->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(1u, sess->acct_record_number);
  delete sess; sess = NULL;

  // INTERIM should keep that session in all the stores.
  _mgr->handle(interim_msg);

  sess = _local_store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);
  delete sess; sess = NULL;

  sess = _remote_store1->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);
  delete sess; sess = NULL;

  sess = _remote_store2->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);
  delete sess; sess = NULL;

  // STOP should remove the session from all the stores.
  _mgr->handle(stop_msg);

  sess = _local_store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  sess = _remote_store1->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  sess = _remote_store2->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);
}

TEST_F(SessionManagerGRTest, InterimUnknownTest)
{
  DummyUnknownErrorPeerMessageSenderFactory* fail_factory = new DummyUnknownErrorPeerMessageSenderFactory(BILLING_REALM);
  SessionManager* fail_mgr = new SessionManager(_local_store,
                                                {_remote_store1, _remote_store2},
                                                _dict,
                                                fail_factory,
                                                _fake_chronos,
                                                _diameter_stack,
                                                _hc);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  Message* interim_msg = new Message("CALL_ID_FOUR", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  _mgr->handle(start_msg);

  // When an INTERIM message fails with a 5002 "Session unknown" error, we
  // should delete the session from all the stores.
  fail_mgr->handle(interim_msg);
  sess = _local_store->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);
  sess = _remote_store1->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);
  sess = _remote_store2->get_session_data("CALL_ID_FOUR", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);

  delete fail_mgr;
  delete fail_factory;
}

TEST_F(SessionManagerGRTest, UnknownCallTest)
{
  SessionStore::Session* sess = NULL;

  Message* interim_msg = new Message("CALL_ID_THREE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 300, FAKE_TRAIL_ID);

  // If we receive an INTERIM for a call not in the store, we should ignore it
  _mgr->handle(interim_msg);
  sess = _local_store->get_session_data("CALL_ID_THREE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);
  sess = _remote_store1->get_session_data("CALL_ID_THREE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);
  sess = _remote_store2->get_session_data("CALL_ID_THREE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, sess);
}

TEST_F(SessionManagerGRTest, EmptyRemoteTest)
{
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  start_msg->ccfs.push_back("10.0.0.1");
  Message* interim_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 0, FAKE_TRAIL_ID);

  // START should put a session in all the stores.
  _mgr->handle(start_msg);

  // Delete the session from a remote store.
  _remote_store1->delete_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);

  // INTERIM should keep that session in all the stores. In particular, it
  // should be back in the remote store we deleted it from.
  _mgr->handle(interim_msg);

  sess = _remote_store1->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);
  delete sess; sess = NULL;
}

TEST_F(SessionManagerGRTest, EmptyLocalTest)
{
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(2), 300, FAKE_TRAIL_ID);
  start_msg->ccfs.push_back("10.0.0.1");
  Message* interim_msg = new Message("CALL_ID_ONE", ORIGINATING, SCSCF, NULL, Rf::AccountingRecordType(3), 0, FAKE_TRAIL_ID);

  // START should put a session in all the stores.
  _mgr->handle(start_msg);

  // Delete the session from the local store.
  _local_store->delete_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);

  // INTERIM should keep that session in all the stores. In particular, it
  // should be back in the remote store we deleted it from.
  _mgr->handle(interim_msg);

  sess = _local_store->get_session_data("CALL_ID_ONE", ORIGINATING, SCSCF, FAKE_TRAIL_ID);
  ASSERT_NE((SessionStore::Session*)NULL, sess);
  EXPECT_EQ(2u, sess->acct_record_number);
  delete sess; sess = NULL;
}
