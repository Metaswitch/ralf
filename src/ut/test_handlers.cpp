/**
 * @file test_handlers.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  ASSERT_NE((Message*)NULL, msg);
  EXPECT_EQ(rc, 200);
  EXPECT_TRUE(msg->record_type.isEvent());
  EXPECT_EQ(msg->ccfs.size(), 1u);
  EXPECT_EQ(msg->ccfs[0], "ec2-54-197-167-141.compute-1.amazonaws.com");
  EXPECT_EQ(msg->session_refresh_time, 300u);
  EXPECT_EQ(msg->timer_interim, false);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, TimerInterimTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", true, body, &msg, FAKE_TRAIL_ID);
  ASSERT_NE((Message*)NULL, msg);
  EXPECT_EQ(rc, 200);
  EXPECT_TRUE(msg->record_type.isEvent());
  EXPECT_EQ(msg->ccfs.size(), 1u);
  EXPECT_EQ(msg->ccfs[0], "ec2-54-197-167-141.compute-1.amazonaws.com");
  EXPECT_EQ(msg->session_refresh_time, 300u);
  EXPECT_EQ(msg->timer_interim, true);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, BadJSONTest)
{
  std::string body = "Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 400);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, NoCCFsTest)
{
  std::string body = "{\"peers\": {\"ccf\": []}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 400);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, InvalidPeersTest)
{
  std::string body = "{\"peers\": {\"ccf\": [77]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 400);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, NoPeerElementTest)
{
  std::string body = "{\"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 200);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, InvalidTypeTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 8, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 400);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, NoTypeTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 400);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, NoIMSInfoTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Acct-Interim-Interval\": 300}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 400);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, NoRoleOfNodeTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Node-Functionality\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 400);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};

TEST_F(HandlerTest, NoNodeFunctionalityTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0}}}}";
  Message* msg = NULL;
  long rc = BillingTask::parse_body("abcd", false, body, &msg, FAKE_TRAIL_ID);
  EXPECT_EQ(rc, 400);
  ASSERT_EQ(NULL, msg);
  delete msg; msg = NULL;
};
