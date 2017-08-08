/**
 * @file test_session_store.cpp UT for Ralf session store.
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

#include "mock_store.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgReferee;

static const SAS::TrailId FAKE_TRAIL = 0;

/// Fixture for BasicSessionStoreTest.
class BasicSessionStoreTest : public ::testing::Test
{
  BasicSessionStoreTest()
  {
    _memstore = new LocalStore();
    _store = new SessionStore(_memstore);
  }

  virtual ~BasicSessionStoreTest()
  {
    delete _store; _store = NULL;
    delete _memstore; _memstore = NULL;
  }

  LocalStore* _memstore;
  SessionStore* _store;
};

TEST_F(BasicSessionStoreTest, SimpleTest)
{
  SessionStore::Session* session = new SessionStore::Session();
  session->session_id = "session_id";
  session->ccf.push_back("ccf1");
  session->ccf.push_back("ccf2");
  session->acct_record_number = 2;
  session->timer_id = "timer_id";
  session->session_refresh_time = 5 * 60;

  // Save the session in the store
  Store::Status rc = this->_store->set_session_data("call_id", ORIGINATING, SCSCF, session, false, FAKE_TRAIL);
  EXPECT_EQ(Store::Status::OK, rc);
  delete session; session = NULL;

  // Retrieve the session again.
  session = this->_store->get_session_data("call_id", ORIGINATING, SCSCF, FAKE_TRAIL);
  ASSERT_TRUE(session != NULL);
  EXPECT_EQ("session_id", session->session_id);
  EXPECT_EQ(2u, session->acct_record_number);
  EXPECT_EQ("timer_id", session->timer_id);
  EXPECT_EQ(5u * 60, session->session_refresh_time);
  EXPECT_EQ(2u, session->ccf.size());

  delete session; session = NULL;
}

TEST_F(BasicSessionStoreTest, DeletionTest)
{
  SessionStore::Session* session = new SessionStore::Session();
  session->session_id = "session_id";
  session->ccf.push_back("ccf1");
  session->ccf.push_back("ccf2");
  session->acct_record_number = 2;
  session->timer_id = "timer_id";
  session->session_refresh_time = 5 * 60;

  // Save the session in the store
  Store::Status rc = this->_store->set_session_data("call_id", ORIGINATING, SCSCF, session, false, FAKE_TRAIL);
  EXPECT_EQ(Store::Status::OK, rc);
  delete session; session = NULL;

  this->_store->delete_session_data("call_id", ORIGINATING, SCSCF, FAKE_TRAIL);

  // Retrieve the session again.
  session = this->_store->get_session_data("call_id", ORIGINATING, SCSCF, FAKE_TRAIL);
  EXPECT_EQ(NULL, session);

  delete session; session = NULL;
}

class SessionStoreCorruptDataTest : public ::testing::Test
{
public:
  void SetUp()
  {
    _memstore = new MockStore();
    _store = new SessionStore(_memstore);
  }

  void TearDown()
  {
    delete _store; _store = NULL;
    delete _memstore; _memstore = NULL;
  }


  MockStore* _memstore;
  SessionStore* _store;
};


TEST_F(SessionStoreCorruptDataTest, BadlyFormedJson)
{
  SessionStore::Session* session;

  EXPECT_CALL(*_memstore, get_data(_, _, _, _, _))
    .WillOnce(DoAll(SetArgReferee<2>(std::string("{ \"session_id: \"12345\"}")),
                    SetArgReferee<3>(1), // CAS
                    Return(Store::OK)));

  session = this->_store->get_session_data("call_id", ORIGINATING, SCSCF, FAKE_TRAIL);
  ASSERT_TRUE(session == NULL);
}


TEST_F(SessionStoreCorruptDataTest, SemanticallyInvalidJson)
{
  SessionStore::Session* session;

  EXPECT_CALL(*_memstore, get_data(_, _, _, _, _))
    .WillOnce(DoAll(SetArgReferee<2>(std::string("{\"session_id\": 12345 }")),
                    SetArgReferee<3>(1), // CAS
                    Return(Store::OK)));

  session = this->_store->get_session_data("call_id", ORIGINATING, SCSCF, FAKE_TRAIL);
  ASSERT_TRUE(session == NULL);
}
