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
    _real_stack->configure(UT_DIR + "/diameterstack.conf");
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
                                                    "session-id",
                                                    "example.com",
                                                    3u,
                                                    body_doc->FindMember("event")->value);
  Diameter::Message msg = launder_message(acr);
  acr = Rf::AccountingRequest(msg);
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
                                                         "example.com",
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
                                                         "example.com",
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
