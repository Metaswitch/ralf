/**
 * @file main.cpp main function for ralf
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <getopt.h>
#include <signal.h>
#include <semaphore.h>
#include <strings.h>
#include <boost/filesystem.hpp>

#include "ralf_pd_definitions.h"

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
#include "astaire_resolver.h"
#include "communicationmonitor.h"
#include "exception_handler.h"
#include "ralf_alarmdefinition.h"
#include "namespace_hop.h"

enum OptionTypes
{
  DNS_SERVER=256+1,
  TARGET_LATENCY_US,
  DIAMETER_TIMEOUT_MS,
  MAX_TOKENS,
  INIT_TOKEN_RATE,
  MIN_TOKEN_RATE,
  MAX_TOKEN_RATE,
  EXCEPTION_MAX_TTL,
  BILLING_PEER,
  HTTP_BLACKLIST_DURATION,
  DIAMETER_BLACKLIST_DURATION,
  ASTAIRE_BLACKLIST_DURATION,
  DNS_TIMEOUT,
  SAS_USE_SIGNALING_IF,
  PIDFILE,
  LOCAL_SITE_NAME,
  SESSION_STORES,
  DAEMON,
  CHRONOS_HOSTNAME,
  RALF_CHRONOS_CALLBACK_URI,
  RALF_HOSTNAME,
  HTTP_ACR_LOGGING,
  RAM_RECORD_EVERYTHING,
};

struct options
{
  std::string local_host;
  std::string local_site_name;
  std::string diameter_conf;
  std::vector<std::string> dns_servers;
  std::vector<std::string> session_stores;
  std::string http_address;
  unsigned short http_port;
  int http_threads;
  std::string billing_realm;
  std::string billing_peer;
  int max_peers;
  bool access_log_enabled;
  std::string access_log_directory;
  bool log_to_file;
  std::string log_directory;
  int log_level;
  std::string sas_server;
  std::string sas_system_name;
  int target_latency_us;
  int diameter_timeout_ms;
  int max_tokens;
  float init_token_rate;
  float min_token_rate;
  float max_token_rate;
  int exception_max_ttl;
  int http_blacklist_duration;
  int diameter_blacklist_duration;
  int astaire_blacklist_duration;
  int dns_timeout;
  std::string pidfile;
  bool daemon;
  bool sas_signaling_if;
  std::string chronos_hostname;
  std::string ralf_chronos_callback_uri;
  std::string ralf_hostname;
  bool http_acr_logging;
  bool ram_record_everything;
};

const static struct option long_opt[] =
{
  {"localhost",                   required_argument, NULL, 'l'},
  {"local-site-name",             required_argument, NULL, LOCAL_SITE_NAME},
  {"diameter-conf",               required_argument, NULL, 'c'},
  {"dns-servers",                 required_argument, NULL, DNS_SERVER},
  {"session-stores",              required_argument, NULL, SESSION_STORES},
  {"http",                        required_argument, NULL, 'H'},
  {"http-threads",                required_argument, NULL, 't'},
  {"billing-realm",               required_argument, NULL, 'b'},
  {"billing-peer",                required_argument, NULL, BILLING_PEER},
  {"max-peers",                   required_argument, NULL, 'p'},
  {"access-log",                  required_argument, NULL, 'a'},
  {"log-file",                    required_argument, NULL, 'F'},
  {"log-level",                   required_argument, NULL, 'L'},
  {"sas",                         required_argument, NULL, 's'},
  {"help",                        no_argument,       NULL, 'h'},
  {"target-latency-us",           required_argument, NULL, TARGET_LATENCY_US},
  {"diameter-timeout-ms",         required_argument, NULL, DIAMETER_TIMEOUT_MS},
  {"max-tokens",                  required_argument, NULL, MAX_TOKENS},
  {"init-token-rate",             required_argument, NULL, INIT_TOKEN_RATE},
  {"min-token-rate",              required_argument, NULL, MIN_TOKEN_RATE},
  {"max-token-rate",              required_argument, NULL, MAX_TOKEN_RATE},
  {"exception-max-ttl",           required_argument, NULL, EXCEPTION_MAX_TTL},
  {"http-blacklist-duration",     required_argument, NULL, HTTP_BLACKLIST_DURATION},
  {"diameter-blacklist-duration", required_argument, NULL, DIAMETER_BLACKLIST_DURATION},
  {"astaire-blacklist-duration",  required_argument, NULL, ASTAIRE_BLACKLIST_DURATION},
  {"dns-timeout",                 required_argument, NULL, DNS_TIMEOUT},
  {"pidfile",                     required_argument, NULL, PIDFILE},
  {"daemon",                      no_argument,       NULL, DAEMON},
  {"sas-use-signaling-interface", no_argument,       NULL, SAS_USE_SIGNALING_IF},
  {"chronos-hostname",            required_argument, NULL, CHRONOS_HOSTNAME},
  {"ralf-chronos-callback-uri",   required_argument, NULL, RALF_CHRONOS_CALLBACK_URI},
  {"ralf-hostname",               required_argument, NULL, RALF_HOSTNAME},
  {"http-acr-logging",            required_argument, NULL, HTTP_ACR_LOGGING},
  { "ram-record-everything",      no_argument,       NULL, RAM_RECORD_EVERYTHING},
  {NULL,                          0,                 NULL, 0},
};

static std::string options_description = "l:c:H:t:b:p:a:F:L:s:h";

void usage(void)
{
  puts("Options:\n"
       "\n"
       " -l, --localhost <hostname> Specify the local hostname or IP address\n"
       "     --local-site-name <name>\n"
       "                            The name of the local site (used in a geo-redundant deployment)\n"
       " -c, --diameter-conf <file> File name for Diameter configuration\n"
       "     --dns-servers <server>[,<server2>,<server3>]\n"
       "                            IP addresses of the DNS servers to use (defaults to 127.0.0.1)\n"
       "     --session-stores <site_name>=<domain>[:<port>][,<site_name>=<domain>[:<port>],...]\n"
       "                            Specifies location of the memcached store in each GR site for storing\n"
       "                            sessions. One of the sites must be the local site. Remote sites for\n"
       "                            geo-redundant storage are optional.\n"
       " -H, --http <address>[:<port>]\n"
       "                            Set HTTP bind address and port (default: 0.0.0.0:8888)\n"
       " -t, --http-threads N       Number of HTTP threads (default: 1)\n"
       " -b, --billing-realm <name> Set Destination-Realm on Rf messages\n"
       "     --billing-peer <name>  If Ralf can't find a CDF by resolving the --billing-realm,\n"
       "                            it will try and connect to this Diameter peer.\n"
       " -p, --max-peers N          Number of peers to connect to (default: 2)\n"
       " -a, --access-log <directory>\n"
       "                            Generate access logs in specified directory\n"
       " -F, --log-file <directory>\n"
       "                            Log to file in specified directory\n"
       " -L, --log-level N          Set log level to N (default: 4)\n"
       " -s, --sas <host>,<system name>\n"
       "                            Use specified host as Service Assurance Server and specified\n"
       "                            system name to identify this system to SAS. If this option isn't\n"
       "                            specified, SAS is disabled\n"
       "     --target-latency-us <usecs>\n"
       "                            Target latency above which throttling applies (default: 100000)\n"
       "     --diameter-timeout <milliseconds>\n"
       "                            Length of time (in ms) before timing out a Diameter request to the CDF\n"
       "     --max-tokens N         Maximum number of tokens allowed in the token bucket (used by\n"
       "                            the throttling code (default: 1000))\n"
       "     --dns-timeout <milliseconds>\n"
       "                            The amount of time to wait for a DNS response (default: 200)n"
       "     --init-token-rate N    Initial token refill rate of tokens in the token bucket (used by\n"
       "                            the throttling code (default: 100.0))\n"
       "     --min-token-rate N     Minimum token refill rate of tokens in the token bucket (used by\n"
       "                            the throttling code (default: 10.0))\n"
       "     --exception-max-ttl <secs>\n"
       "                            The maximum time before the process exits if it hits an exception.\n"
       "                            The actual time is randomised.\n"
       "     --http-blacklist-duration <secs>\n"
       "                            The amount of time to blacklist an HTTP peer when it is unresponsive.\n"
       "     --diameter-blacklist-duration <secs>\n"
       "                            The amount of time to blacklist a Diameter peer when it is unresponsive.\n"
       "     --astaire-blacklist-duration <secs>\n"
       "                            The amount of time to blacklist an Astaire node when it is unresponsive.\n"
       "     --sas-use-signaling-interface\n"
       "                            Whether SAS traffic is to be dispatched over the signaling network\n"
       "                            interface rather than the default management interface\n"
       "     --chronos-hostname <hostname>\n"
       "                            The hostname of the remote Chronos cluster to use. If unset, the default\n"
       "                            is to use localhost, using localhost as the callback URL.\n"
       "     --ralf-chronos-callback-uri <hostname>\n"
       "                            The ralf hostname used for Chronos callbacks. If unset the default \n"
       "                            is to use the ralf-hostname.\n"
       "                            Ignored if chronos-hostname is not set.\n"
       "     --ralf-hostname <hostname:port>\n"
       "                            The hostname and port of the cluster of Ralf nodes to which this Ralf is\n"
       "                            a member. The port should be the HTTP port the nodes are listening on.\n"
       "                            This is used to form the callback URL for the Chronos cluser.\n"
       "     --http-acr-logging     Whether to include the bodies of ACR HTTP requests when they are logged\n"
       "                            to SAS\n"
       "     --ram-record-everything\n"
       "                            Write all logs to RAM and dump them to file on abnormal termination\n"
       "     --pidfile=<filename>   Write pidfile\n"
       "     --daemon               Run as a daemon\n"
       " -h, --help                 Show this help screen\n"
      );
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

    case DAEMON:
      options.daemon = true;
      break;

    case RAM_RECORD_EVERYTHING:
      options.ram_record_everything = true;
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
  bool diameter_timeout_set = false;

  optind = 0;
  while ((opt = getopt_long(argc, argv, options_description.c_str(), long_opt, &long_opt_ind)) != -1)
  {
    switch (opt)
    {
    case 'l':
      TRC_INFO("Local host: %s", optarg);
      options.local_host = std::string(optarg);
      break;

    case 'c':
      TRC_INFO("Diameter configuration file: %s", optarg);
      options.diameter_conf = std::string(optarg);
      break;

    case LOCAL_SITE_NAME:
      options.local_site_name = std::string(optarg);
      TRC_INFO("Local site name = %s", optarg);
      break;

    case SESSION_STORES:
      {
        // This option has the format
        // <site_name>=<domain>,[<site_name>=<domain>,<site_name=<domain>,...].
        // For now, just split into a vector of <site_name>=<domain> strings. We
        // need to know the local site name to parse this properly, so we'll do
        // that later.
        std::string stores_arg = std::string(optarg);
        boost::split(options.session_stores,
                     stores_arg,
                     boost::is_any_of(","));
      }
      break;

    case 'H':
      TRC_INFO("HTTP address: %s", optarg);
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
        TRC_INFO("SAS set to %s", options.sas_server.c_str());
        TRC_INFO("System name is set to %s", options.sas_system_name.c_str());
      }
      else
      {
        TRC_WARNING("Invalid --sas option: %s", optarg);
      }
    }
    break;

    case 't':
      TRC_INFO("HTTP threads: %s", optarg);
      options.http_threads = atoi(optarg);
      break;

    case 'b':
      TRC_INFO("Billing realm: %s", optarg);
      options.billing_realm = std::string(optarg);
      break;

    case BILLING_PEER:
      TRC_INFO("Fallback Diameter peer to connect to: %s", optarg);
      options.billing_peer = std::string(optarg);
      break;

    case 'p':
      TRC_INFO("Maximum peers: %s", optarg);
      options.max_peers = atoi(optarg);
      break;

    case 'a':
      TRC_INFO("Access log: %s", optarg);
      options.access_log_enabled = true;
      options.access_log_directory = std::string(optarg);
      break;

    case DNS_SERVER:
      options.dns_servers.clear();
      Utils::split_string(std::string(optarg), ',', options.dns_servers, 0, false);
      TRC_INFO("%d DNS servers passed on the command line",
               options.dns_servers.size());
      break;

    case 'F':
    case 'L':
    case DAEMON:
    case RAM_RECORD_EVERYTHING:
      // Ignore options that are handled by init_logging_options
      break;

    case 'h':
      usage();
      return -1;

    case TARGET_LATENCY_US:
      options.target_latency_us = atoi(optarg);
      if (options.target_latency_us <= 0)
      {
        TRC_ERROR("Invalid --target-latency-us option %s", optarg);
        return -1;
      }
      break;

    case DIAMETER_TIMEOUT_MS:
      TRC_INFO("Diameter timeout: %s", optarg);
      diameter_timeout_set = true;
      options.diameter_timeout_ms = atoi(optarg);
      break;

    case MAX_TOKENS:
      options.max_tokens = atoi(optarg);
      if (options.max_tokens <= 0)
      {
        TRC_ERROR("Invalid --max-tokens option %s", optarg);
        return -1;
      }
      break;

    case INIT_TOKEN_RATE:
      options.init_token_rate = atoi(optarg);
      if (options.init_token_rate <= 0)
      {
        TRC_ERROR("Invalid --init-token-rate option %s", optarg);
        return -1;
      }
      break;

    case MIN_TOKEN_RATE:
      options.min_token_rate = atoi(optarg);
      if (options.min_token_rate <= 0)
      {
        TRC_ERROR("Invalid --min-token-rate option %s", optarg);
        return -1;
      }
      break;

    case MAX_TOKEN_RATE:
      options.max_token_rate = atoi(optarg);
      if (options.max_token_rate < 0)
      {
        TRC_ERROR("Invalid --max-token-rate option %s", optarg);
        return -1;
      }
      break;

    case EXCEPTION_MAX_TTL:
      options.exception_max_ttl = atoi(optarg);
      TRC_INFO("Max TTL after an exception set to %d",
               options.exception_max_ttl);
      break;

    case HTTP_BLACKLIST_DURATION:
      options.http_blacklist_duration = atoi(optarg);
      TRC_INFO("HTTP blacklist duration set to %d",
               options.http_blacklist_duration);
      break;

    case DIAMETER_BLACKLIST_DURATION:
      options.diameter_blacklist_duration = atoi(optarg);
      TRC_INFO("Diameter blacklist duration set to %d",
               options.diameter_blacklist_duration);
      break;

    case ASTAIRE_BLACKLIST_DURATION:
      options.astaire_blacklist_duration = atoi(optarg);
      TRC_INFO("Astaire blacklist duration set to %d",
               options.astaire_blacklist_duration);
      break;

    case DNS_TIMEOUT:
      options.dns_timeout = atoi(optarg);
      TRC_INFO("DNS timeout set to %d", options.dns_timeout);
      break;

    case PIDFILE:
      options.pidfile = std::string(optarg);
      break;

    case SAS_USE_SIGNALING_IF:
      options.sas_signaling_if = true;
      break;

    case CHRONOS_HOSTNAME:
      options.chronos_hostname = std::string(optarg);
      break;

    case RALF_CHRONOS_CALLBACK_URI:
      options.ralf_chronos_callback_uri = std::string(optarg);
      break;

    case RALF_HOSTNAME:
      options.ralf_hostname = std::string(optarg);
      break;

    case HTTP_ACR_LOGGING:
      options.http_acr_logging = true;
      break;

    default:
      CL_RALF_INVALID_OPTION_C.log();
      TRC_ERROR("Unknown option: %d.  Run with --help for options.", opt);
      return -1;
    }
  }

  if (!diameter_timeout_set)
  {
    Utils::calculate_diameter_timeout(options.target_latency_us,
                                      options.diameter_timeout_ms);
  }

  return 0;
}

static sem_t term_sem;
ExceptionHandler* exception_handler;

// Signal handler that triggers homestead termination.
void terminate_handler(int sig)
{
  sem_post(&term_sem);
}

// Signal handler that simply dumps the stack and then crashes out.
void signal_handler(int sig)
{
  // Reset the signal handlers so that another exception will cause a crash.
  signal(SIGABRT, SIG_DFL);
  signal(SIGSEGV, signal_handler);

  // Log the signal, along with a simple backtrace.
  TRC_BACKTRACE("Signal %d caught", sig);

  // Check if there's a stored jmp_buf on the thread and handle if there is
  exception_handler->handle_exception();

  //
  // If we get here it means we didn't handle the exception so we need to exit.
  //

  CL_RALF_CRASHED.log(strsignal(sig));

  // Log a full backtrace to make debugging easier.
  TRC_BACKTRACE_ADV();

  // Ensure the log files are complete - the core file created by abort() below
  // will trigger the log files to be copied to the diags bundle
  TRC_COMMIT();

  RamRecorder::dump("/var/log/ralf");

  // Dump a core.
  abort();
}

int main(int argc, char**argv)
{
  // Set up our exception signal handler for asserts and segfaults.
  signal(SIGABRT, signal_handler);
  signal(SIGSEGV, signal_handler);

  sem_init(&term_sem, 0, 0);
  signal(SIGTERM, terminate_handler);

  struct options options;
  options.local_host = "127.0.0.1";
  options.diameter_conf = "/var/lib/ralf/ralf.conf";
  options.dns_servers.push_back("127.0.0.1");
  options.http_address = "0.0.0.0";
  options.http_port = 10888;
  options.http_threads = 1;
  options.billing_realm = "dest-realm.unknown";
  options.billing_peer = "";
  options.max_peers = 2;
  options.access_log_enabled = false;
  options.log_to_file = false;
  options.log_level = 0;
  options.sas_server = "0.0.0.0";
  options.sas_system_name = "";
  options.target_latency_us = 100000;
  options.diameter_timeout_ms = 200;
  options.max_tokens = 1000;
  options.init_token_rate = 100.0;
  options.min_token_rate = 10.0;
  options.max_token_rate = 0.0;
  options.exception_max_ttl = 600;
  options.http_blacklist_duration = HttpResolver::DEFAULT_BLACKLIST_DURATION;
  options.diameter_blacklist_duration = DiameterResolver::DEFAULT_BLACKLIST_DURATION;
  options.astaire_blacklist_duration = AstaireResolver::DEFAULT_BLACKLIST_DURATION;
  options.session_stores = {"127.0.0.1"};
  options.dns_timeout = DnsCachedResolver::DEFAULT_TIMEOUT;
  options.pidfile = "";
  options.daemon = false;
  options.sas_signaling_if = false;
  options.ram_record_everything = false;

  if (init_logging_options(argc, argv, options) != 0)
  {
    return 1;
  }

  Utils::daemon_log_setup(argc,
                          argv,
                          options.daemon,
                          options.log_directory,
                          options.log_level,
                          options.log_to_file);

  if (options.ram_record_everything)
  {
    TRC_INFO("RAM record everything enabled");
    RamRecorder::recordEverything();
  }

  // We should now have a connection to syslog so we can write the started ENT
  // log.
  CL_RALF_STARTED.log();

  std::stringstream options_ss;
  for (int ii = 0; ii < argc; ii++)
  {
    options_ss << argv[ii];
    options_ss << " ";
  }
  std::string options_str = "Command-line options were: " + options_ss.str();

  TRC_INFO(options_str.c_str());

  if (init_options(argc, argv, options) != 0)
  {
    return 1;
  }

  if (options.pidfile != "")
  {
    int rc = Utils::lock_and_write_pidfile(options.pidfile);
    if (rc == -1)
    {
      // Failure to acquire pidfile lock
      TRC_ERROR("Could not write pidfile - exiting");
      return 2;
    }
  }

  // Check we've been provided with a hostname
  if (options.ralf_hostname == "")
  {
    TRC_ERROR("No Ralf hostname provided - exiting");
    return 1;
  }

  // Parse the session-stores argument.
  std::string session_store_location;
  std::vector<std::string> remote_session_stores_locations;

  if (!Utils::parse_multi_site_stores_arg(options.session_stores,
                                          options.local_site_name,
                                          "session-stores",
                                          session_store_location,
                                          remote_session_stores_locations))
  {
    return 1;
  }

  start_signal_handlers();

  if (options.sas_server == "0.0.0.0")
  {
    TRC_WARNING("SAS server option was invalid or not configured - SAS is disabled");
    CL_RALF_INVALID_SAS_OPTION.log();
  }

  // Create Ralf's alarm objects. Note that the alarm identifier strings must match those
  // in the alarm definition JSON file exactly.
  AlarmManager* alarm_manager = new AlarmManager();

  CommunicationMonitor* cdf_comm_monitor = new CommunicationMonitor(new Alarm(alarm_manager,
                                                                              "ralf",
                                                                              AlarmDef::RALF_CDF_COMM_ERROR,
                                                                              AlarmDef::CRITICAL),
                                                                    "Ralf",
                                                                    "CDF");

  CommunicationMonitor* chronos_comm_monitor = new CommunicationMonitor(new Alarm(alarm_manager,
                                                                                  "ralf",
                                                                                  AlarmDef::RALF_CHRONOS_COMM_ERROR,
                                                                                  AlarmDef::CRITICAL),
                                                                        "Ralf",
                                                                        "Chronos");

  CommunicationMonitor* astaire_comm_monitor = new CommunicationMonitor(new Alarm(alarm_manager,
                                                                                  "ralf",
                                                                                  AlarmDef::RALF_ASTAIRE_COMM_ERROR,
                                                                                  AlarmDef::CRITICAL),
                                                                          "Ralf",
                                                                          "Astaire");

  CommunicationMonitor* remote_astaire_comm_monitor = new CommunicationMonitor(new Alarm(alarm_manager,
                                                                                         "ralf",
                                                                                         AlarmDef::RALF_REMOTE_ASTAIRE_COMM_ERROR,
                                                                                         AlarmDef::CRITICAL),
                                                                               "Ralf",
                                                                               "remote Astaire");

  AccessLogger* access_logger = NULL;
  if (options.access_log_enabled)
  {
    access_logger = new AccessLogger(options.access_log_directory);
  }

  SAS::init(options.sas_system_name,
            "ralf",
            SASEvent::CURRENT_RESOURCE_BUNDLE,
            options.sas_server,
            sas_write,
            options.sas_signaling_if ? create_connection_in_signaling_namespace
                                     : create_connection_in_management_namespace);

  LoadMonitor* load_monitor = new LoadMonitor(options.target_latency_us,
                                              options.max_tokens,
                                              options.init_token_rate,
                                              options.min_token_rate,
                                              options.max_token_rate);

  HealthChecker* hc = new HealthChecker();
  hc->start_thread();

  // Create an exception handler. The exception handler doesn't need
  // to quiesce the process before killing it.
  exception_handler = new ExceptionHandler(options.exception_max_ttl,
                                           false,
                                           hc);

  Diameter::Stack* diameter_stack = Diameter::Stack::get_instance();
  Rf::Dictionary* dict = NULL;

  try
  {
    diameter_stack->initialize();
    diameter_stack->configure(options.diameter_conf,
                              exception_handler,
                              cdf_comm_monitor);
    dict = new Rf::Dictionary();
    diameter_stack->advertize_application(Diameter::Dictionary::Application::ACCT,
                                          dict->RF);
    diameter_stack->start();
  }
  catch (Diameter::Stack::Exception& e)
  {
    CL_RALF_DIAMETER_INIT_FAIL.log(e._func, e._rc);
    TRC_ERROR("Failed to initialize Diameter stack - function %s, rc %d", e._func, e._rc);
    exit(2);
  }

  // Create a DNS resolver.  We'll use this for HTTP, for Diameter and for Astaire.
  DnsCachedResolver* dns_resolver = new DnsCachedResolver(options.dns_servers,
                                                          options.dns_timeout);

  int addr_family = AF_INET;
  struct in6_addr dummy_addr_resolver;
  if (inet_pton(AF_INET6, options.local_host.c_str(), &dummy_addr_resolver) == 1)
  {
    TRC_DEBUG("Local host is an IPv6 address");
    addr_family = AF_INET6;
  }

  AstaireResolver* astaire_resolver =
                        new AstaireResolver(dns_resolver,
                                            addr_family,
                                            options.astaire_blacklist_duration);

  TopologyNeutralMemcachedStore* local_memstore =
                      new TopologyNeutralMemcachedStore(session_store_location,
                                                        astaire_resolver,
                                                        false,
                                                        astaire_comm_monitor);

  SessionStore* local_session_store = new SessionStore(local_memstore);

  std::vector<Store*> remote_memstores;
  std::vector<SessionStore*> remote_session_stores;

  for (std::vector<std::string>::iterator it = remote_session_stores_locations.begin();
       it != remote_session_stores_locations.end();
       ++it)
  {
    TopologyNeutralMemcachedStore* remote_memstore =
                     new TopologyNeutralMemcachedStore(*it,
                                                       astaire_resolver,
                                                       true,
                                                       remote_astaire_comm_monitor);
    remote_memstores.push_back(remote_memstore);
    SessionStore* remote_session_store = new SessionStore(remote_memstore);
    remote_session_stores.push_back(remote_session_store);
  }

  BillingHandlerConfig* cfg = new BillingHandlerConfig();
  PeerMessageSenderFactory* factory = new PeerMessageSenderFactory(options.billing_realm,
                                                                   options.diameter_timeout_ms);

  // Create a connection to Chronos.
  std::string port_str = std::to_string(options.http_port);

  std::string chronos_service;
  std::string chronos_callback_addr = "127.0.0.1:" + port_str;
  int http_af = AF_INET;

  if (options.chronos_hostname == "")
  {
    // If we are using a local chronos cluster, we want Chronos to call back to
    // its local Ralf instance so that we can handle Ralfs failing without missing
    // timers.
    chronos_service = "127.0.0.1:7253";

    Utils::IPAddressType address_type = Utils::parse_ip_address(options.http_address);

    if ((address_type == Utils::IPAddressType::IPV6_ADDRESS) ||
        (address_type == Utils::IPAddressType::IPV6_ADDRESS_WITH_PORT) ||
        (address_type == Utils::IPAddressType::IPV6_ADDRESS_BRACKETED))
    {
      http_af = AF_INET6;
      chronos_callback_addr = "[::1]:" + port_str;
    }
  }
  else
  {
    chronos_service = options.chronos_hostname + ":7253";

    Utils::IPAddressType address_type = Utils::parse_ip_address(options.chronos_hostname);

    if ((address_type == Utils::IPAddressType::IPV6_ADDRESS) ||
        (address_type == Utils::IPAddressType::IPV6_ADDRESS_WITH_PORT) ||
        (address_type == Utils::IPAddressType::IPV6_ADDRESS_BRACKETED))
    {
      http_af = AF_INET6;
    }

    if (options.ralf_chronos_callback_uri != "")
    {
      // The callback URI doesn't include the port
      chronos_callback_addr = options.ralf_chronos_callback_uri + ":" + port_str;
    }
    else
    {
      chronos_callback_addr = options.ralf_hostname;
    }
  }

  // Create a connection to Chronos.  This requires building the HttpResolver,
  // Client, and Connection, to pass into the ChronosConnection.
  TRC_STATUS("Creating connection to Chronos at %s using %s as the callback URI",
             chronos_service.c_str(), chronos_callback_addr.c_str());

  HttpResolver* http_resolver = new HttpResolver(dns_resolver,
                                                 http_af,
                                                 options.http_blacklist_duration);

  HttpClient* chronos_http_client = new HttpClient(false,
                                                   http_resolver,
                                                   SASEvent::HttpLogLevel::DETAIL,
                                                   chronos_comm_monitor);

  HttpConnection* chronos_http_conn = new HttpConnection(chronos_service,
                                                         chronos_http_client);

  ChronosConnection* timer_conn = new ChronosConnection(chronos_callback_addr,
                                                        chronos_http_conn);

  cfg->mgr = new SessionManager(local_session_store, remote_session_stores, dict, factory, timer_conn, diameter_stack, hc);

  HttpStack* http_stack = new HttpStack(options.http_threads,
                                        exception_handler,
                                        access_logger,
                                        load_monitor);
  HttpStackUtils::PingHandler ping_handler;
  BillingHandler billing_handler(cfg, options.http_acr_logging);
  try
  {
    http_stack->initialize();
    http_stack->bind_tcp_socket(options.http_address,
                                options.http_port);
    http_stack->register_handler("^/ping$", &ping_handler);
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
    TRC_DEBUG("Local host is an IPv6 address");
    diameter_af = AF_INET6;
  }

  DiameterResolver* diameter_resolver = new DiameterResolver(dns_resolver,
                                                             diameter_af,
                                                             options.diameter_blacklist_duration);
  RealmManager* realm_manager = new RealmManager(diameter_stack,
                                                 options.billing_realm,
                                                 options.billing_peer,
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

  try
  {
    diameter_stack->stop();
    diameter_stack->wait_stopped();
  }
  catch (Diameter::Stack::Exception& e)
  {
    CL_RALF_DIAMETER_STOP_FAIL.log(e._func, e._rc);
    TRC_ERROR("Failed to stop Diameter stack - function %s, rc %d", e._func, e._rc);
  }

  realm_manager->stop();

  delete realm_manager; realm_manager = NULL;
  delete diameter_resolver; diameter_resolver = NULL;
  delete timer_conn; timer_conn = NULL;
  delete chronos_http_conn; chronos_http_conn = NULL;
  delete chronos_http_client; chronos_http_client = NULL;
  delete http_resolver; http_resolver = NULL;
  delete dns_resolver; dns_resolver = NULL;
  delete load_monitor; load_monitor = NULL;

  delete local_session_store; local_session_store = NULL;
  delete local_memstore; local_memstore = NULL;
  for (std::vector<SessionStore*>::iterator it = remote_session_stores.begin();
       it != remote_session_stores.end();
       ++it)
  {
    delete *it;
  }
  remote_session_stores.clear();
  for (std::vector<Store*>::iterator it = remote_memstores.begin();
       it != remote_memstores.end();
       ++it)
  {
    delete *it;
  }
  remote_memstores.clear();
  delete astaire_resolver; astaire_resolver = NULL;

  hc->stop_thread();
  delete exception_handler; exception_handler = NULL;
  delete hc; hc = NULL;
  delete http_stack; http_stack = NULL;

  // Delete Ralf's alarm objects
  delete cdf_comm_monitor;
  delete chronos_comm_monitor;
  delete astaire_comm_monitor;
  delete remote_astaire_comm_monitor;
  delete alarm_manager;

  signal(SIGTERM, SIG_DFL);
  sem_destroy(&term_sem);
}
