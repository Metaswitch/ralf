/**
 * @file test_handlers.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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

#include "handlers.hpp"
#include "message.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

static const SAS::TrailId FAKE_TRAIL_ID = 0;

class HandlerTest : public ::testing::Test
{
  HandlerTest()
  {
  }

  virtual ~HandlerTest()
  {
  }
};

TEST_F(HandlerTest, GoodJSONTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_NE((Message*)NULL, msg);
  EXPECT_TRUE(msg->record_type.isEvent());
  EXPECT_EQ(msg->ccfs.size(), 1u);
  EXPECT_EQ(msg->ccfs[0], "ec2-54-197-167-141.compute-1.amazonaws.com");
  EXPECT_EQ(msg->session_refresh_time, 300u);
  EXPECT_EQ(msg->timer_interim, false);
  delete msg;
};

TEST_F(HandlerTest, TimerInterimTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", true, body, FAKE_TRAIL_ID);
  ASSERT_NE((Message*)NULL, msg);
  EXPECT_TRUE(msg->record_type.isEvent());
  EXPECT_EQ(msg->ccfs.size(), 1u);
  EXPECT_EQ(msg->ccfs[0], "ec2-54-197-167-141.compute-1.amazonaws.com");
  EXPECT_EQ(msg->session_refresh_time, 300u);
  EXPECT_EQ(msg->timer_interim, true);
  delete msg;
};

TEST_F(HandlerTest, BadJSONTest)
{
  std::string body = "Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};

TEST_F(HandlerTest, NoCCFsTest)
{
  std::string body = "{\"peers\": {\"ccf\": []}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};

TEST_F(HandlerTest, InvalidPeersTest)
{
  std::string body = "{\"peers\": {\"ccf\": [77]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};

TEST_F(HandlerTest, NoPeerElementTest)
{
  std::string body = "{\"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};

TEST_F(HandlerTest, InvalidTypeTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 8, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};

TEST_F(HandlerTest, NoTypeTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};

TEST_F(HandlerTest, NoIMSInfoTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Acct-Interim-Interval\": 300}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};

TEST_F(HandlerTest, NoRoleOfNodeTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Node-Functionality\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};

TEST_F(HandlerTest, NoNodeFunctionalityTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0}}}}";
  Message* msg = BillingControllerTask::parse_body("abcd", false, body, FAKE_TRAIL_ID);
  ASSERT_EQ(NULL, msg);
};
