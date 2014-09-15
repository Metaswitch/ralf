/**
 * @file ralfdcea.cpp - Craft Description, Cause, Effect, and Action
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
#include "craft_dcea.h"
// Ralf syslog identities
/*************************************************************/
SysLog CL_RALF_INVALID_SAS_OPTION
{
  CL_RALF_ID + 1,
  SYSLOG_ERR,
  "Invalid --sas option in /etc/clearwater/config, SAS disabled",
  "",
  "",
  ""
};
SysLog CL_RALF_INVALID_OPTION_C
{
  CL_RALF_ID + 2,
  SYSLOG_ERR,
  "Fatal - Unknown command line option %c.  Run with --help for options.",
  "",
  "",
  ""
};
SysLog1<char*> CL_RALF_CRASHED
{
  CL_RALF_ID + 3,
  SYSLOG_ERR,
  "Fatal - Ralf has exited or crashed with signal %s",
  "",
  "",
  ""
};
SysLog CL_RALF_STARTED
{
  CL_RALF_ID + 4,
  SYSLOG_ERR,
  "Ralf started",
  "",
  "",
  ""
};
SysLog2<char*, int> CL_RALF_HTTP_ERROR
{
  CL_RALF_ID + 5,
  SYSLOG_ERR,
  "The Http stack has encountered an error in function %s with error %d",
  "",
  "",
  ""
};
SysLog CL_RALF_ENDED
{
  CL_RALF_ID + 6,
  SYSLOG_ERR,
  "Ralf ended - Termination signal received - terminating",
  "",
  "",
  ""
};
SysLog2<char*, int> CL_RALF_HTTP_STOP_ERROR
{
  CL_RALF_ID + 7,
  SYSLOG_ERR,
  "Failed to stop HttpStack stack in function %s with error %d",
  "",
  "",
  ""
};
