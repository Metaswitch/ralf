TARGETS := ralf
TEST_TARGETS := ralf_test

COMMON_SOURCES := accesslogger.cpp \
                  localstore.cpp \
                  memcachedstore.cpp \
                  memcachedstoreview.cpp \
                  memcached_config.cpp \
                  signalhandler.cpp \
                  sessionstore.cpp \
                  session_manager.cpp \
                  peer_message_sender.cpp \
                  diameterstack.cpp \
                  rf.cpp \
                  ralf_transaction.cpp \
                  message.cpp \
                  httpstack.cpp \
                  httpstack_utils.cpp \
                  handlers.cpp \
                  logger.cpp \
                  saslogger.cpp \
                  httpresolver.cpp \
                  httpconnection.cpp \
                  chronosconnection.cpp \
                  load_monitor.cpp \
                  statistic.cpp \
                  utils.cpp \
                  zmq_lvc.cpp \
                  log.cpp \
                  baseresolver.cpp \
                  diameterresolver.cpp \
                  dnscachedresolver.cpp \
                  dnsparser.cpp \
                  realmmanager.cpp \
                  ipv6utils.cpp \
                  alarm.cpp \
                  base_communication_monitor.cpp \
                  communicationmonitor.cpp \
                  exception_handler.cpp \
                  health_checker.cpp \
                  counter.cpp \
                  namespace_hop.cpp

ralf_SOURCES := ${COMMON_SOURCES} main.cpp
ralf_test_SOURCES := ${COMMON_SOURCES} \
	                   test_sessionstore.cpp \
                     test_session_manager.cpp \
                     test_rf.cpp \
                     test_handlers.cpp \
                     test_main.cpp \
                     fakelogger.cpp \
										 mock_chronos_connection.cpp

COMMON_CPPFLAGS := -I../include \
                   -I../usr/include \
									 -I../modules/cpp-common/include \
									 -I../modules/sas-client/include \
									 -I../modules/jsoncpp/include \
									 -I../modules/rapidjson/include

ralf_CPPFLAGS := ${COMMON_CPPFLAGS}
ralf_test_CPPFLAGS := ${COMMON_CPPFLAGS}

COMMON_LDFLAGS := -L../usr/lib -lzmq -lpthread -lrt -lcares -lsas -lz -lmemcached -lboost_regex -lboost_date_time -lboost_system -lboost_filesystem -lcurl -levhtp -levent -lfdproto -lfdcore -levent_pthreads
ralf_LDFLAGS := ${COMMON_LDFLAGS}
ralf_test_LDFLAGS := ${COMMON_LDFLAGS}

VPATH += ../modules/cpp-common/src ./ut ../modules/cpp-common/test_utils

include ../build-infra/cpp.mk
