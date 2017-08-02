/**
 * @file test_handlers.cpp
 *
 * Copyright (C) Metaswitch Networks 2014
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
#include "test_utils.hpp"

#include "mockdiameterstack.hpp"
#include "mockhttpstack.hpp"
#include "mock_health_checker.hpp"
#include "localstore.h"
#include "fakechronosconnection.cpp"
#include "session_store.h"
#include "peer_message_sender_factory.hpp"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;
using ::testing::An;

static const SAS::TrailId FAKE_TRAIL_ID = 0;
static const std::string CALL_ID = "abc123";

class HandlerTest : public ::testing::Test
{
  static Diameter::Stack* _real_stack;
  static MockDiameterStack* _mock_stack;
  static MockHttpStack* _httpstack;
  static SessionManager* _mgr;
  static BillingHandlerConfig* _cfg;
  static LocalStore* _local_data_store;
  static SessionStore* _local_session_store;
  static LocalStore* _remote_data_store;
  static SessionStore* _remote_session_store;
  static FakeChronosConnection* _chronos_connection;
  static MockHealthChecker* _hc;
  static PeerMessageSenderFactory* _factory;
  static Rf::Dictionary* _dict;

  static struct msg* _caught_fd_msg;
  static Diameter::Transaction* _caught_diam_tsx;

  HandlerTest()
  {
  }

  virtual ~HandlerTest()
  {
  }

  static void SetUpTestCase()
  {
    _real_stack = Diameter::Stack::get_instance();
    _real_stack->initialize();
    _real_stack->configure(UT_DIR + "/diameterstack.conf", NULL);

    _mock_stack = new MockDiameterStack();
    _httpstack = new MockHttpStack();
    _cfg = new BillingHandlerConfig();
    _local_data_store = new LocalStore();
    _local_session_store = new SessionStore(_local_data_store);
    _remote_data_store = new LocalStore();
    _remote_session_store = new SessionStore(_remote_data_store);
    _dict = new Rf::Dictionary();
    _factory = new PeerMessageSenderFactory("example.com");
    _chronos_connection = new FakeChronosConnection();
    _hc = new MockHealthChecker();
    _mgr = new SessionManager(_local_session_store, { _remote_session_store }, _dict, _factory, _chronos_connection, _mock_stack, _hc);
    _cfg->mgr = _mgr;
  }

  static void TearDownTestCase()
  {
    _real_stack->stop();
    _real_stack->wait_stopped();

    delete _mock_stack; _mock_stack = NULL;
    delete _httpstack; _httpstack = NULL;
    delete _mgr, _mgr = NULL;
    delete _cfg, _cfg = NULL;
    delete _local_data_store; _local_data_store = NULL;
    delete _local_session_store; _local_session_store = NULL;
    delete _remote_data_store; _remote_data_store = NULL;
    delete _remote_session_store; _remote_session_store = NULL;
    delete _chronos_connection; _chronos_connection = NULL;
    delete _hc; _hc = NULL;
    delete _dict; _dict = NULL;
    delete _factory; _factory = NULL;
  }

  void TearDown()
  {
    if (_caught_diam_tsx != NULL)
    {
      delete _caught_diam_tsx; _caught_diam_tsx = NULL;
    }
  }

  static void store_msg_tsx(struct msg* msg, Diameter::Transaction* tsx)
  {
    if (_caught_diam_tsx != NULL)
    {
      delete _caught_diam_tsx; _caught_diam_tsx = NULL;
    }

    _caught_fd_msg = msg;
    _caught_diam_tsx = tsx;
  }

  void request_response_template(int result_code,
                                 bool timer_interim)
  {
    std::string query = "";
    if (timer_interim)
    {
      query += "?timer-interim=true";
    }

    std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";

    // Send in a mock HTTP resuest.
    MockHttpStack::Request req(_httpstack,
                               "/call-id/" + CALL_ID,
                               "",
                               query,
                               body,
                               htp_method_POST);

    BillingTask* task = new BillingTask(req,
                                        _cfg,
                                        FAKE_TRAIL_ID);

    // Running the handler should trigger an HTTP response and a Diameter
    // request.
    EXPECT_CALL(*_httpstack, send_reply(_, 200, _));
    EXPECT_CALL(*_mock_stack, send(_, An<Diameter::Transaction*>()))
      .Times(1)
      .WillOnce(WithArgs<0,1>(Invoke(store_msg_tsx)));;
    task->run();

    Rf::AccountingResponse aca(_dict,
                               _mock_stack,
                               result_code,
                               CALL_ID);

    // Send in a response.
    if (result_code == 2001)
    {
      EXPECT_CALL(*_hc, health_check_passed());
    }

    fd_msg_free(_caught_fd_msg); _caught_fd_msg = NULL;
    _caught_diam_tsx->on_response(aca);
  }

  void request_timeout_template(bool multiple_peers)
  {
    std::string body;
    if (multiple_peers)
    {
      body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\", \"ec2-34-189-147-119.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";

    }
    else
    {
      body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";
    }

    // Send in a mock HTTP resuest.
    MockHttpStack::Request req(_httpstack,
                               "/call-id/" + CALL_ID,
                               "",
                               "",
                               body,
                               htp_method_POST);

    BillingTask* task = new BillingTask(req,
                                        _cfg,
                                        FAKE_TRAIL_ID);

    // Running the handler should trigger an HTTP response and a Diameter
    // request.
    EXPECT_CALL(*_httpstack, send_reply(_, 200, _));
    EXPECT_CALL(*_mock_stack, send(_, An<Diameter::Transaction*>()))
      .Times(1)
      .WillOnce(WithArgs<0,1>(Invoke(store_msg_tsx)));;
    task->run();

    // If we have multiple peers, expect a retransmission when we time out the
    // first request.
    if (multiple_peers)
    {
      EXPECT_CALL(*_mock_stack, send(_, An<Diameter::Transaction*>()))
        .Times(1)
        .WillOnce(WithArgs<0,1>(Invoke(store_msg_tsx)));;
    }

    fd_msg_free(_caught_fd_msg); _caught_fd_msg = NULL;
    _caught_diam_tsx->on_timeout();

    if (multiple_peers)
    {
      // Time out the retransmission.
      fd_msg_free(_caught_fd_msg); _caught_fd_msg = NULL;
      _caught_diam_tsx->on_timeout();
    }
  }
};

Diameter::Stack* HandlerTest::_real_stack;
MockDiameterStack* HandlerTest::_mock_stack = NULL;
MockHttpStack* HandlerTest::_httpstack = NULL;
SessionManager* HandlerTest::_mgr = NULL;
BillingHandlerConfig* HandlerTest::_cfg = NULL;
LocalStore* HandlerTest::_local_data_store = NULL;
SessionStore* HandlerTest::_local_session_store;
LocalStore* HandlerTest::_remote_data_store = NULL;
SessionStore* HandlerTest::_remote_session_store;
FakeChronosConnection* HandlerTest::_chronos_connection = NULL;
MockHealthChecker* HandlerTest::_hc = NULL;
Rf::Dictionary* HandlerTest::_dict = NULL;
PeerMessageSenderFactory* HandlerTest::_factory = NULL;

struct msg* HandlerTest::_caught_fd_msg;
Diameter::Transaction* HandlerTest::_caught_diam_tsx;

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

TEST_F(HandlerTest, NotPost)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}}";

  // Send in a mock HTTP resuest. It's not a POST so should be rejected with a
  // 405 response.
  MockHttpStack::Request req(_httpstack,
                             "/call-id/skdlfjsdkf",
                             "",
                             "",
                             body,
                             htp_method_PUT);

  BillingTask* task = new BillingTask(req,
                                      _cfg,
                                      FAKE_TRAIL_ID);

  // Running the handler should trigger an 405 HTTP response.
  EXPECT_CALL(*_httpstack, send_reply(_,405, _));
  task->run();
}

TEST_F(HandlerTest, BadBody)
{
  std::string body = "{\"peers\": {\"ccf\": [\"ec2-54-197-167-141.compute-1.amazonaws.com\"]}, \"event\": {\"Accounting-Record-Type\": 1, \"Acct-Interim-Interval\": 300, \"Service-Information\": {\"IMS-Information\": {\"Role-Of-Node\": 0, \"Node-Functionality\": 0}}}";

  // Send in a mock HTTP resuest. The body is invalid JSON so it should trigger
  // a 405 response.
  MockHttpStack::Request req(_httpstack,
                             "/call-id/" + CALL_ID,
                             "",
                             "",
                             body,
                             htp_method_POST);

  BillingTask* task = new BillingTask(req,
                                      _cfg,
                                      FAKE_TRAIL_ID);

  // Running the handler should trigger a 405 HTTP response.
  EXPECT_CALL(*_httpstack, send_reply(_, 400, _));
  task->run();
}

TEST_F(HandlerTest, SimpleMainline)
{
  request_response_template(2001, false);
}

TEST_F(HandlerTest, SimpleInterim)
{
  request_response_template(2001, true);
}

TEST_F(HandlerTest, DeliveryFailed)
{
  request_response_template(3002, false);
}

TEST_F(HandlerTest, Timeout)
{
  request_timeout_template(false);
}

TEST_F(HandlerTest, TimeoutMultiplePeers)
{
  request_timeout_template(true);
}
