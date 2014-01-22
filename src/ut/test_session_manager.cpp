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


class SessionManagerTest : public ::testing::Test
{
  SessionManagerTest()
  {
  }

  virtual ~SessionManagerTest()
  {
  }
};

TEST_F(SessionManagerTest, SimpleTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  SessionManager* mgr = new SessionManager(store);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_ONE", NULL, "START");
  Message* interim_msg = new Message("CALL_ID_ONE", NULL, "INTERIM");
  Message* stop_msg = new Message("CALL_ID_ONE", NULL, "STOP");

  mgr->handle(start_msg);

  sess = store->get_session_data("CALL_ID_ONE");
  ASSERT_EQ(1u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  mgr->handle(interim_msg);

  sess = store->get_session_data("CALL_ID_ONE");
  ASSERT_EQ(2u, sess->acct_record_number);
  delete sess;
  sess = NULL;

  mgr->handle(stop_msg);

  sess = store->get_session_data("CALL_ID_ONE");
  ASSERT_EQ(NULL, sess);

  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, NewCallTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  SessionManager* mgr = new SessionManager(store);
  SessionStore::Session* sess = NULL;

  Message* start_msg = new Message("CALL_ID_TWO", NULL, "START");
  Message* interim_msg = new Message("CALL_ID_TWO", NULL, "INTERIM");
  Message* stop_msg = new Message("CALL_ID_TWO", NULL, "STOP");
  Message* start_msg_2 = new Message("CALL_ID_TWO", NULL, "START");

  mgr->handle(start_msg);

  sess = store->get_session_data("CALL_ID_TWO");
  delete sess;
  mgr->handle(stop_msg);
  sess = store->get_session_data("CALL_ID_TWO");
  ASSERT_EQ(NULL, sess);

  mgr->handle(start_msg_2);

  sess = store->get_session_data("CALL_ID_TWO");
  printf("%ld\n", sess);
  ASSERT_EQ(1u, sess->acct_record_number);
  delete sess;
  sess = NULL;


  delete store;
  delete memstore;
}

TEST_F(SessionManagerTest, UnknownCallTest)
{
  LocalStore* memstore = new LocalStore();
  SessionStore* store = new SessionStore(memstore);
  SessionManager* mgr = new SessionManager(store);
  SessionStore::Session* sess = NULL;

  Message* interim_msg = new Message("CALL_ID_THREE", NULL, "INTERIM");

  mgr->handle(interim_msg);
  sess = store->get_session_data("CALL_ID_THREE");
  ASSERT_EQ(NULL, sess);

  delete store;
  delete memstore;
}
