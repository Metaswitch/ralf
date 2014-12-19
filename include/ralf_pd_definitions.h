/**
 * @file ralf_pd_definitions.h  Defines instances of PDLog for Ralf
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

#ifndef _RALF_PD_DEFINITIONS_H__
#define _RALF_PD_DEFINITIONS_H__

#include <string>
#include "pdlog.h"

// Defines instances of PDLog for Ralf

// The fields for each PDLog instance contains:
//   Identity - Identifies the log id to be used in the syslog id field.
//   Severity - One of Emergency, Alert, Critical, Error, Warning, Notice, 
//              and Info.  Directly corresponds to the syslog severity types.
//              Only PDLOG_ERROR or PDLOG_NOTICE are used.  
//              See syslog_facade.h for definitions.
//   Message  - Formatted description of the condition.
//   Cause    - The cause of the condition.
//   Effect   - The effect the condition.
//   Action   - A list of one or more actions to take to resolve the condition 
//              if it is an error.
const static PDLog CL_RALF_INVALID_SAS_OPTION
(
  PDLogBase::CL_RALF_ID + 1,
  PDLOG_INFO,
  "The sas_server option in /etc/clearwater/config is invalid or "
  "not configured.",
  "The interface to the SAS is not specified.",
  "No call traces will appear in the SAS.",
  "Set the fully qualified sas hostname for the sas_server=<hostname> option. "
);

const static PDLog CL_RALF_INVALID_OPTION_C
(
  PDLogBase::CL_RALF_ID + 2,
  PDLOG_ERR,
  "Fatal - Unknown command line option %c.  Run with --help for options.",
  "There was an invalid command line option in /etc/clearwater/config",
  "The application will exit and restart until the problem is fixed.",
  "Correct the /etc/clearwater/config file."
);

const static PDLog1<const char*> CL_RALF_CRASHED
(
  PDLogBase::CL_RALF_ID + 3,
  PDLOG_ERR,
  "Fatal - Ralf has exited or crashed with signal %s.",
  "Ralf has encountered a fatal software error or has been terminated",
  "The application will exit and restart until the problem is fixed.",
  "This error can occur if Ralf has been terminated by operator command. "
  "Check your installation and configuration for other types of crashes."
);

const static PDLog CL_RALF_STARTED
(
  PDLogBase::CL_RALF_ID + 4,
  PDLOG_ERR,
  "Ralf started.",
  "The Ralf application is starting.",
  "Normal.",
  "None."
);

const static PDLog2<const char*, int> CL_RALF_HTTP_ERROR
(
  PDLogBase::CL_RALF_ID + 5,
  PDLOG_ERR,
  "The HTTP stack has encountered an error in function %s with error %d.",
  "Ralf encountered an error when attempting to make an HTTP connection "
  "to Chronos.",
  "The interface to Chronos has failed.  Ralf can't use timer services.",
  "Check the /etc/clearwater/config for Chronos."
);

const static PDLog CL_RALF_ENDED
(
  PDLogBase::CL_RALF_ID + 6,
  PDLOG_ERR,
  "Ralf ended - Termination signal received - terminating.",
  "Ralf has been terminated by Monit or has exited.",
  "Ralf billing service is not longer available.",
  "(1). This occurs normally when Ralf is stopped. "
  "(2). If Ralf failed to respond then monit can restart Ralf. "
);

const static PDLog2<const char*, int> CL_RALF_HTTP_STOP_ERROR
(
  PDLogBase::CL_RALF_ID + 7,
  PDLOG_ERR,
  "Failed to stop HTTP stack in function %s with error %d.",
  "When Ralf was exiting it encountered an error when shutting "
  "down the HTTP stack.",
  "Not critical as Ralf is exiting anyway.",
  "None."
);

#endif
