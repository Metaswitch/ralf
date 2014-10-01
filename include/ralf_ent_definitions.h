/**
 * @file ralf_ent_definitions.h  Sprout ENT Log declarations.
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

#ifndef _RALFDCEA_H__
#define _RALFSTEADDCEA_H__

#include <string>
#include "craft_ent_definitions.h"

// Ralf syslog identities
/*************************************************************/
const static PDLog CL_RALF_INVALID_SAS_OPTION
(
  PDLogBase::CL_RALF_ID + 1,
  PDLOG_ERR,
  "The sas_server option in /etc/clearwater/config is invalid or not configured",
  "The interface to the SAS is not specified.",
  "No call traces will appear in the SAS",
  "Set the fully qualified sas hostname for the sas_server=<host> option. "
  "Consult the Installation document."
);
const static PDLog CL_RALF_INVALID_OPTION_C
(
  PDLogBase::CL_RALF_ID + 2,
  PDLOG_ERR,
  "Fatal - Unknown command line option %c.  Run with --help for options.",
  "There was an invalid command line option in /etc/clearwater/config",
  "Ralf will exit",
  "Correct the /etc/clearwater/config file."
);
const static PDLog1<const char*> CL_RALF_CRASHED
(
  PDLogBase::CL_RALF_ID + 3,
  PDLOG_ERR,
  "Fatal - Ralf has exited or crashed with signal %s",
  "Ralf has encountered a fatal software error or has been terminated",
  "The Ralf application will restart.",
  "This error can occur if Ralf has been terminated by operator command. "
  "Crashes such as segment trap, bus error trap, should be reported to support. "
);
const static PDLog CL_RALF_STARTED
(
  PDLogBase::CL_RALF_ID + 4,
  PDLOG_ERR,
  "Ralf started",
  "The Ralf application is starting.",
  "Normal",
  "None"
);
const static PDLog2<const char*, int> CL_RALF_HTTP_ERROR
(
  PDLogBase::CL_RALF_ID + 5,
  PDLOG_ERR,
  "The HTTP stack has encountered an error in function %s with error %d",
  "Ralf encountered an error when attempting to make an HTTP connection to Chronos.",
  "The interface to Chronos has failed.  Ralf can't use timer services.",
  "Report this issue to support."
);
const static PDLog CL_RALF_ENDED
(
  PDLogBase::CL_RALF_ID + 6,
  PDLOG_ERR,
  "Ralf ended - Termination signal received - terminating",
  "Ralf has been terminated by Monit or has exited",
  "Ralf billing service is not longer available",
  "(1). This occurs normally when Ralf is stopped. "
  "(2). If Ralf failed to respond then monit can restart Ralf.  Report this issue to support."
);
const static PDLog2<const char*, int> CL_RALF_HTTP_STOP_ERROR
(
  PDLogBase::CL_RALF_ID + 7,
  PDLOG_ERR,
  "Failed to stop HTTPStack stack in function %s with error %d",
  "When Ralf was exiting it encountered an error when shutting down the HTTP stack.",
  "Not critical as Ralf is exiting anyway.",
  "Report this issue to support."
);

#endif
