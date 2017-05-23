/**
 * @file ralf_pd_definitions.h  Defines instances of PDLog for Ralf
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef _RALF_PD_DEFINITIONS_H__
#define _RALF_PD_DEFINITIONS_H__

#include <string>
#include "pdlog.h"

// Defines instances of PDLog for Ralf

// The fields for each PDLog instance contains:
//   Identity - Identifies the log id to be used in the syslog id field.
//   Severity - One of Emergency, Alert, Critical, Error, Warning, Notice, 
//              and Info. Only LOG_ERROR or LOG_NOTICE are used.  
//   Message  - Formatted description of the condition.
//   Cause    - The cause of the condition.
//   Effect   - The effect the condition.
//   Action   - A list of one or more actions to take to resolve the condition 
//              if it is an error.
const static PDLog CL_RALF_INVALID_SAS_OPTION
(
  PDLogBase::CL_RALF_ID + 1,
  LOG_INFO,
  "The sas_server option in /etc/clearwater/config is invalid or "
  "not configured.",
  "The interface to the SAS is not specified.",
  "No call traces will appear in the SAS.",
  "Set the fully qualified sas hostname for the sas_server=<hostname> option. "
);

const static PDLog CL_RALF_INVALID_OPTION_C
(
  PDLogBase::CL_RALF_ID + 2,
  LOG_ERR,
  "Fatal - Unknown command line option %c.  Run with --help for options.",
  "There was an invalid command line option in /etc/clearwater/config",
  "The application will exit and restart until the problem is fixed.",
  "Correct the /etc/clearwater/config file."
);

const static PDLog1<const char*> CL_RALF_CRASHED
(
  PDLogBase::CL_RALF_ID + 3,
  LOG_ERR,
  "Fatal - Ralf has exited or crashed with signal %s.",
  "Ralf has encountered a fatal software error or has been terminated",
  "The application will exit and restart until the problem is fixed.",
  "Ensure that Ralf has been installed correctly and that it "
  "has valid configuration."
);

const static PDLog CL_RALF_STARTED
(
  PDLogBase::CL_RALF_ID + 4,
  LOG_ERR,
  "Ralf started.",
  "The Ralf application is starting.",
  "Normal.",
  "None."
);

const static PDLog2<const char*, int> CL_RALF_HTTP_ERROR
(
  PDLogBase::CL_RALF_ID + 5,
  LOG_ERR,
  "The HTTP stack has encountered an error in function %s with error %d.",
  "Ralf encountered an error when attempting to make an HTTP connection "
  "to Chronos.",
  "The interface to Chronos has failed.  Ralf can't use timer services.",
  "Check the /etc/clearwater/config for Chronos."
);

const static PDLog CL_RALF_ENDED
(
  PDLogBase::CL_RALF_ID + 6,
  LOG_ERR,
  "Ralf ended - Termination signal received - terminating.",
  "Ralf has been terminated by Monit or has exited.",
  "Ralf billing service is not longer available.",
  "(1). This occurs normally when Ralf is stopped. "
  "(2). If Ralf failed to respond then monit can restart Ralf. "
);

const static PDLog2<const char*, int> CL_RALF_HTTP_STOP_ERROR
(
  PDLogBase::CL_RALF_ID + 7,
  LOG_ERR,
  "Failed to stop HTTP stack in function %s with error %d.",
  "When Ralf was exiting it encountered an error when shutting "
  "down the HTTP stack.",
  "Not critical as Ralf is exiting anyway.",
  "None."
);

static const PDLog2<const char*, int> CL_RALF_DIAMETER_INIT_FAIL
(
  PDLogBase::CL_RALF_ID + 8,
  LOG_ERR,
  "Fatal - Failed to initialize Diameter stack in function %s with error %d.",
  "The Diameter interface could not be initialized or encountered an "
  "error while running.",
  "The application will exit and restart until the problem is fixed.",
  "(1). Check the configuration for the Diameter destination hosts. "
  "(2). Check the connectivity to the Diameter host using Wireshark."
);

static const PDLog2<const char*, int> CL_RALF_DIAMETER_STOP_FAIL
(
  PDLogBase::CL_RALF_ID + 9,
  LOG_ERR,
  "Failed to stop Diameter stack in function %s with error %d.",
  "The Diameter interface encountered an error when shutting "
  "down the Diameter interface.",
  "Not critical as Ralf is exiting anyway.",
  "No action required."
);

#endif
