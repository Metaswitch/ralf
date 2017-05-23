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

#include <stdexcept>

#include "rf.h"
#include "ralf_transaction.hpp"
#include "message.hpp"
#include "diameterstack.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "rapidjson/rapidjson.h"
#include "test_utils.hpp"

class RfTest : public ::testing::Test
{
  void SetUp()
  {
    _real_stack = Diameter::Stack::get_instance();
    _real_stack->initialize();
    _real_stack->configure(UT_DIR + "/diameterstack.conf", NULL);
    _dict = new Rf::Dictionary();
  }

  void TearDown()
  {
    _real_stack->stop();
    _real_stack->wait_stopped();
    delete _dict;
  }

private:
  Diameter::Stack* _real_stack;
  Rf::Dictionary* _dict;

  Diameter::Message launder_message(const Diameter::Message& msg)
  {
    struct msg* msg_to_build = msg.fd_msg();
    uint8_t* buffer;
    size_t len;
    int rc = fd_msg_bufferize(msg_to_build, &buffer, &len);
    if (rc != 0)
    {
      std::stringstream ss;
      ss << "fd_msg_bufferize failed: " << rc;
      throw new std::runtime_error(ss.str());
    }

    struct msg* parsed_msg = NULL;
    rc = fd_msg_parse_buffer(&buffer, len, &parsed_msg);
    if (rc != 0)
    {
      std::stringstream ss;
      ss << "fd_msg_parse_buffer failed: " << rc;
      throw new std::runtime_error(ss.str());
    }

    struct fd_pei error_info;
    rc = fd_msg_parse_dict(parsed_msg, fd_g_config->cnf_dict, &error_info);
    if (rc != 0)
    {
      std::stringstream ss;
      ss << "fd_msg_parse_dict failed: " << rc << " - " << error_info.pei_errcode;
      throw new std::runtime_error(ss.str());
    }

    return Diameter::Message(_dict, parsed_msg, _real_stack);
  }
};

TEST_F(RfTest, CreateMessageTest)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300}}";
  rapidjson::Document* body_doc = new rapidjson::Document();
  body_doc->Parse<0>(body.c_str());
  ASSERT_TRUE(body_doc->IsObject());

  Rf::AccountingRequest acr = Rf::AccountingRequest(_dict,
                                                    _real_stack,
                                                    "example-session-id",
                                                    "host.example.com",
                                                    "realm.example.com",
                                                    3u,
                                                    body_doc->FindMember("event")->value);
  Diameter::Message msg = launder_message(acr);
  acr = Rf::AccountingRequest(msg);

  delete body_doc;
};

TEST_F(RfTest, DISABLED_SuccessTransactionTest)
{
  Diameter::Stack* diameter_stack = Diameter::Stack::get_instance();
  Rf::Dictionary* dict = new Rf::Dictionary();
  RalfTransaction tsx(dict, NULL, NULL, 0);
  diameter_stack->start();
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Result-Code\": 2001}}";
  rapidjson::Document* body_doc = new rapidjson::Document();
  body_doc->Parse<0>(body.c_str());
  ASSERT_TRUE(body_doc->IsObject());
  Rf::AccountingRequest* msg = new Rf::AccountingRequest(dict,
                                                         diameter_stack,
                                                         "session_id",
                                                         "host.example.com",
                                                         "realm.example.com",
                                                         3u,
                                                         body_doc->FindMember("event")->value);
  tsx.on_response(*msg);
  // assert that a fake SessionManager was called with accepted = true
};

TEST_F(RfTest, DISABLED_FailureTransactionTest)
{
  Diameter::Stack* diameter_stack = Diameter::Stack::get_instance();
  Rf::Dictionary* dict = new Rf::Dictionary();
  RalfTransaction tsx(dict, NULL, NULL, 0);
  diameter_stack->start();
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Result-Code\": 3001}}";  rapidjson::Document* body_doc = new rapidjson::Document();
  body_doc->Parse<0>(body.c_str());
  ASSERT_TRUE(body_doc->IsObject());
  Rf::AccountingRequest* msg = new Rf::AccountingRequest(dict,
                                                         diameter_stack,
                                                         "session_id",
                                                         "host.example.com",
                                                         "realm.example.com",
                                                         3u,
                                                         body_doc->FindMember("event")->value);
  tsx.on_response(*msg);
  // assert that a fake SessionManager was called with accepted = false
};

TEST_F(RfTest, DISABLED_TimeoutTransactionTest)
{
  Diameter::Stack* diameter_stack = Diameter::Stack::get_instance();
  Rf::Dictionary* dict = new Rf::Dictionary();
  RalfTransaction tsx(dict, NULL, NULL, 0);
  diameter_stack->start();
  tsx.on_timeout();
  // assert that a fake SessionManager was called with accepted = false
};
