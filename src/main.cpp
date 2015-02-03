/**
 * @file main.cpp main function for ralf
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

#include <getopt.h>
#include <signal.h>
#include <semaphore.h>
#include <strings.h>
#include <boost/filesystem.hpp>

#include "ralf_pd_definitions.h"

#include "ipv6utils.h"
#include "memcachedstore.h"
#include "httpresolver.h"
#include "chronosconnection.h"
#include "accesslogger.h"
#include "log.h"
#include "utils.h"
#include "httpstack.h"
#include "handlers.hpp"
#include "logger.h"
#include "saslogger.h"
#include "rf.h"
#include "peer_message_sender_factory.hpp"
#include "sas.h"
#include "load_monitor.h"
#include "diameterresolver.h"
#include "realmmanager.h"
#include "communicationmonitor.h"

enum OptionTypes
{
  ALARMS_ENABLED=256+1,
  DNS_SERVER
};

struct options
{
  std::string local_host;
  std::string diameter_conf;
  std::string dns_server;
  std::string http_address;
  unsigned short http_port;
  int http_threads;
  std::string billing_realm;
  int max_peers;
  bool access_log_enabled;
  std::string access_log_directory;
  bool log_to_file;
  std::string log_directory;
  int log_level;
  std::string sas_server;
  std::string sas_system_name;
  bool alarms_enabled;
};

const static struct option long_opt[] =
{
  {"localhost",         required_argument, NULL, 'l'},
  {"diameter-conf",     required_argument, NULL, 'c'},
  {"dns-server",        required_argument, NULL, DNS_SERVER},
  {"http",              required_argument, NULL, 'H'},
  {"http-threads",      required_argument, NULL, 't'},
  {"billing-realm",     required_argument, NULL, 'b'},
  {"max-peers",         required_argument, NULL, 'p'},
  {"access-log",        required_argument, NULL, 'a'},
  {"log-file",          required_argument, NULL, 'F'},
  {"log-level",         required_argument, NULL, 'L'},
  {"sas",               required_argument, NULL, 's'},
  {"alarms-enabled",    no_argument,       NULL, ALARMS_ENABLED},
  {"help",              no_argument,       NULL, 'h'},
  {NULL,                0,                 NULL, 0},
};

static std::string options_description = "l:c:H:t:b:p:a:F:L:s:h";

void usage(void)
{
  puts("Options:\n"
       "\n"
       " -l, --localhost <hostname> Specify the local hostname or IP address\n"
       " -c, --diameter-conf <file> File name for Diameter configuration\n"
       "     --dns-server <IP>      DNS server to use to resolve addresses\n"
       " -H, --http <address>[:<port>]\n"
       "                            Set HTTP bind address and port (default: 0.0.0.0:8888)\n"
       " -t, --http-threads N       Number of HTTP threads (default: 1)\n"
       " -b, --billing-realm <name> Set Destination-Realm on Rf messages\n"
       " -p, --max-peers N          Number of peers to connect to (default: 2)\n"
       " -a, --access-log <directory>\n"
       "                            Generate access logs in specified directory\n"
       " -F, --log-file <directory>\n"
       "                            Log to file in specified directory\n"
       " -L, --log-level N          Set log level to N (default: 4)\n"
       " -s, --sas <host>,<system name>\n"
       " Use specified host as Service Assurance Server and specified\n"
       " system name to identify this system to SAS. If this option isn't\n"
       " specified, SAS is disabled\n"
       "     --alarms-enabled       Whether SNMP alarms are enabled (default: false)\n"
       " -h, --help                 Show this help screen\n");
}

int init_logging_options(int argc, char**argv, struct options& options)
{
  int opt;
  int long_opt_ind;

  optind = 0;
  while ((opt = getopt_long(argc, argv, options_description.c_str(), long_opt, &long_opt_ind)) != -1)
  {
    switch (opt)
    {
    case 'F':
      options.log_to_file = true;
      options.log_directory = std::string(optarg);
      break;

    case 'L':
      options.log_level = atoi(optarg);
      break;

    default:
      // Ignore other options at this point
      break;
    }
  }

  return 0;
}

int init_options(int argc, char**argv, struct options& options)
{
  int opt;
  int long_opt_ind;

  optind = 0;
  while ((opt = getopt_long(argc, argv, options_description.c_str(), long_opt, &long_opt_ind)) != -1)
  {
    switch (opt)
    {
    case 'l':
      LOG_INFO("Local host: %s", optarg);
      options.local_host = std::string(optarg);
      break;

    case 'c':
      LOG_INFO("Diameter configuration file: %s", optarg);
      options.diameter_conf = std::string(optarg);
      break;

    case 'H':
      LOG_INFO("HTTP address: %s", optarg);
      options.http_address = std::string(optarg);
      // TODO: Parse optional HTTP port.
      break;

    case 's':
    {
      std::vector<std::string> sas_options;
      Utils::split_string(std::string(optarg), ',', sas_options, 0, false);
      if ((sas_options.size() == 2) &&
          !sas_options[0].empty() &&
          !sas_options[1].empty())
      {
        options.sas_server = sas_options[0];
        options.sas_system_name = sas_options[1];
        LOG_INFO("SAS set to %s\n", options.sas_server.c_str());
        LOG_INFO("System name is set to %s\n", options.sas_system_name.c_str());
      }
      else
      {
        CL_RALF_INVALID_SAS_OPTION.log();
        LOG_WARNING("Invalid --sas option, SAS disabled\n");
      }
    }
    break;

    case 't':
      LOG_INFO("HTTP threads: %s", optarg);
      options.http_threads = atoi(optarg);
      break;

    case 'b':
      LOG_INFO("Billing realm: %s", optarg);
      options.billing_realm = std::string(optarg);
      break;

    case 'p':
      LOG_INFO("Maximum peers: %s", optarg);
      options.max_peers = atoi(optarg);
      break;

    case 'a':
      LOG_INFO("Access log: %s", optarg);
      options.access_log_enabled = true;
      options.access_log_directory = std::string(optarg);
      break;

    case ALARMS_ENABLED:
      LOG_INFO("SNMP alarms are enabled");
      options.alarms_enabled = true;
      break;

    case DNS_SERVER:
      LOG_INFO("DNS server set to %s", optarg);
      options.dns_server = std::string(optarg);
      break;

    case 'F':
    case 'L':
      // Ignore F and L - these are handled by init_logging_options
      break;

    case 'h':
      usage();
      return -1;

    default:
      CL_RALF_INVALID_OPTION_C.log();
      LOG_ERROR("Unknown option: %d.  Run with --help for options.\n", opt);
      return -1;
    }
  }

  return 0;
}

static sem_t term_sem;

// Signal handler that triggers homestead termination.
void terminate_handler(int sig)
{
  sem_post(&term_sem);
}

// Signal handler that simply dumps the stack and then crashes out.
void exception_handler(int sig)
{
  // Reset the signal handlers so that another exception will cause a crash.
  signal(SIGABRT, SIG_DFL);
  signal(SIGSEGV, SIG_DFL);

  // Log the signal, along with a backtrace.
  CL_RALF_CRASHED.log(strsignal(sig));
  closelog();
  LOG_BACKTRACE("Signal %d caught", sig);

  // Ensure the log files are complete - the core file created by abort() below
  // will trigger the log files to be copied to the diags bundle
  LOG_COMMIT();

  // Dump a core.
  abort();
}

int main(int argc, char**argv)
{
  CommunicationMonitor* cdf_comm_monitor = NULL;
  CommunicationMonitor* chronos_comm_monitor = NULL;
  CommunicationMonitor* memcached_comm_monitor = NULL;
  Alarm* vbucket_alarm = NULL;

  // Set up our exception signal handler for asserts and segfaults.
  signal(SIGABRT, exception_handler);
  signal(SIGSEGV, exception_handler);

  sem_init(&term_sem, 0, 0);
  signal(SIGTERM, terminate_handler);

  struct options options;
  options.local_host = "127.0.0.1";
  options.diameter_conf = "/var/lib/ralf/ralf.conf";
  options.dns_server = "127.0.0.1";
  options.http_address = "0.0.0.0";
  options.http_port = 10888;
  options.http_threads = 1;
  options.billing_realm = "dest-realm.unknown";
  options.max_peers = 2;
  options.access_log_enabled = false;
  options.log_to_file = false;
  options.log_level = 0;
  options.sas_server = "0.0.0.0";
  options.sas_system_name = "";
  options.alarms_enabled = false;

  boost::filesystem::path p = argv[0];
  openlog(p.filename().c_str(), PDLOG_PID, PDLOG_LOCAL6);
  CL_RALF_STARTED.log();

  if (init_logging_options(argc, argv, options) != 0)
  {
    closelog();
    return 1;
  }

  Log::setLoggingLevel(options.log_level);
  if ((options.log_to_file) && (options.log_directory != ""))
  {
    // Work out the program name from argv[0], stripping anything before the final slash.
    char* prog_name = argv[0];
    char* slash_ptr = rindex(argv[0], '/');
    if (slash_ptr != NULL)
    {
      prog_name = slash_ptr + 1;
    }
    Log::setLogger(new Logger(options.log_directory, prog_name));
  }

  LOG_STATUS("Log level set to %d", options.log_level);

  std::stringstream options_ss;
  for (int ii = 0; ii < argc; ii++)
  {
    options_ss << argv[ii];
    options_ss << " ";
  }
  std::string options_str = "Command-line options were: " + options_ss.str();

  LOG_INFO(options_str.c_str());

  if (init_options(argc, argv, options) != 0)
  {
    closelog();
    return 1;
  }

  if (options.alarms_enabled)
  {
    // Create Ralf's alarm objects. Note that the alarm identifier strings must match those
    // in the alarm definition JSON file exactly.

    cdf_comm_monitor = new CommunicationMonitor(new Alarm("ralf", AlarmDef::RALF_CDF_COMM_ERROR,
                                                                  AlarmDef::CRITICAL));

    chronos_comm_monitor = new CommunicationMonitor(new Alarm("ralf", AlarmDef::RALF_CHRONOS_COMM_ERROR,
                                                                      AlarmDef::CRITICAL));

    memcached_comm_monitor = new CommunicationMonitor(new Alarm("ralf", AlarmDef::RALF_MEMCACHED_COMM_ERROR,
                                                                        AlarmDef::CRITICAL));

    vbucket_alarm = new Alarm("ralf", AlarmDef::RALF_VBUCKET_ERROR,
                                      AlarmDef::MAJOR);

    // Start the alarm request agent
    AlarmReqAgent::get_instance().start();
    AlarmState::clear_all("ralf");
  }

  MemcachedStore* mstore = new MemcachedStore(false, 
                                              "./cluster_settings",
                                              memcached_comm_monitor,
                                              vbucket_alarm);

  if (!(mstore->has_servers()))
  {
    LOG_ERROR("./cluster_settings file does not contain a valid set of servers");
    return 1;
  };

  AccessLogger* access_logger = NULL;
  if (options.access_log_enabled)
  {
    access_logger = new AccessLogger(options.access_log_directory);
  }

  SAS::init(options.sas_system_name,
            "ralf",
            SASEvent::CURRENT_RESOURCE_BUNDLE,
            options.sas_server,
            sas_write);

  LoadMonitor* load_monitor = new LoadMonitor(100000, // Initial target latency (us)
                                              20, // Maximum token bucket size.
                                              10.0, // Initial token fill rate (per sec).
                                              10.0); // Minimum token fill rate (pre sec).

  Diameter::Stack* diameter_stack = Diameter::Stack::get_instance();
  Rf::Dictionary* dict = NULL;
  diameter_stack->initialize();
  diameter_stack->configure(options.diameter_conf, cdf_comm_monitor);
  dict = new Rf::Dictionary();
  diameter_stack->advertize_application(Diameter::Dictionary::Application::ACCT,
                                        dict->RF);
  diameter_stack->start();

  SessionStore* store = new SessionStore(mstore);
  BillingHandlerConfig* cfg = new BillingHandlerConfig();
  PeerMessageSenderFactory* factory = new PeerMessageSenderFactory(options.billing_realm);

  // Create a DNS resolver.  We'll use this both for HTTP and for Diameter.
  DnsCachedResolver* dns_resolver = new DnsCachedResolver(options.dns_server);

  std::string port_str = std::to_string(options.http_port);

  // We want Chronos to call back to its local sprout instance so that we can
  // handle Ralfs failing without missing timers.
  int http_af = AF_INET;
  std::string chronos_callback_addr = "127.0.0.1:" + port_str;
  std::string local_chronos = "127.0.0.1:7253";
  if (is_ipv6(options.http_address))
  {
    http_af = AF_INET6;
    chronos_callback_addr = "[::1]:" + port_str;
    local_chronos = "[::1]:7253";
  }

  // Create a connection to Chronos.  This requires an HttpResolver.
  LOG_STATUS("Creating connection to Chronos at %s using %s as the callback URI", local_chronos.c_str(), chronos_callback_addr.c_str());
  HttpResolver* http_resolver = new HttpResolver(dns_resolver, http_af);
  ChronosConnection* timer_conn = new ChronosConnection(local_chronos, chronos_callback_addr, http_resolver, chronos_comm_monitor);
  cfg->mgr = new SessionManager(store, dict, factory, timer_conn, diameter_stack);

  HttpStack* http_stack = HttpStack::get_instance();
  HttpStackUtils::PingHandler ping_handler;
  BillingHandler billing_handler(cfg);
  try
  {
    http_stack->initialize();
    http_stack->configure(options.http_address, options.http_port, options.http_threads, access_logger, load_monitor);
    http_stack->register_handler("^/ping$", & ping_handler);
    http_stack->register_handler("^/call-id/[^/]*$", &billing_handler);
    http_stack->start();
  }
  catch (HttpStack::Exception& e)
  {
    CL_RALF_HTTP_ERROR.log(e._func, e._rc);
    fprintf(stderr, "Caught HttpStack::Exception - %s - %d\n", e._func, e._rc);
  }

  // Create a DNS resolver and a Diameter specific resolver.
  int diameter_af = AF_INET;
  struct in6_addr dummy_addr;
  if (inet_pton(AF_INET6, options.local_host.c_str(), &dummy_addr) == 1)
  {
    LOG_DEBUG("Local host is an IPv6 address");
    diameter_af = AF_INET6;
  }

  DiameterResolver* diameter_resolver = new DiameterResolver(dns_resolver, diameter_af);
  RealmManager* realm_manager = new RealmManager(diameter_stack,
                                                 options.billing_realm,
                                                 options.max_peers,
                                                 diameter_resolver);
  realm_manager->start();

  sem_wait(&term_sem);

  CL_RALF_ENDED.log();
  try
  {
    http_stack->stop();
    http_stack->wait_stopped();
  }
  catch (HttpStack::Exception& e)
  {
    CL_RALF_HTTP_STOP_ERROR.log(e._func, e._rc);
    fprintf(stderr, "Caught HttpStack::Exception - %s - %d\n", e._func, e._rc);
  }

  realm_manager->stop();
  delete realm_manager; realm_manager = NULL;
  delete diameter_resolver; diameter_resolver = NULL;
  delete http_resolver; http_resolver = NULL;
  delete dns_resolver; dns_resolver = NULL;

  delete load_monitor; load_monitor = NULL;

  if (options.alarms_enabled)
  {
    // Stop the alarm request agent
    AlarmReqAgent::get_instance().stop();

    // Delete Ralf's alarm objects
    delete cdf_comm_monitor;
    delete chronos_comm_monitor;
    delete memcached_comm_monitor;
    delete vbucket_alarm;
  }

  closelog();
  signal(SIGTERM, SIG_DFL);
  sem_destroy(&term_sem);
}
