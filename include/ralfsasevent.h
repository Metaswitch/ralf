/**
 * @file ralfsasevent.h Ralf SAS event IDs
 *
 * project clearwater - ims in the cloud
 * copyright (c) 2013  metaswitch networks ltd
 *
 * this program is free software: you can redistribute it and/or modify it
 * under the terms of the gnu general public license as published by the
 * free software foundation, either version 3 of the license, or (at your
 * option) any later version, along with the "special exception" for use of
 * the program along with ssl, set forth below. this program is distributed
 * in the hope that it will be useful, but without any warranty;
 * without even the implied warranty of merchantability or fitness for
 * a particular purpose.  see the gnu general public license for more
 * details. you should have received a copy of the gnu general public
 * license along with this program.  if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * the author can be reached by email at clearwater@metaswitch.com or by
 * post at metaswitch networks ltd, 100 church st, enfield en2 6bq, uk
 *
 * special exception
 * metaswitch networks ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining openssl with the
 * software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the gpl. you must comply with the gpl in all
 * respects for all of the code used other than openssl.
 * "openssl" means openssl toolkit software distributed by the openssl
 * project and licensed under the openssl licenses, or a work based on such
 * software and licensed under the openssl licenses.
 * "openssl licenses" means the openssl license and original ssleay license
 * under which the openssl project distributes the openssl toolkit software,
 * as those licenses appear in the file license-openssl.
 */

#ifndef RALFSASEVENT_H__
#define RALFSASEVENT_H__

#include "sasevent.h"

namespace SASEvent 
{
  //----------------------------------------------------------------------------
  // Ralf events
  //----------------------------------------------------------------------------

  const int NEW_RF_SESSION = RALF_BASE + 0x000;
  const int CONTINUED_RF_SESSION = RALF_BASE + 0x100;
  const int END_RF_SESSION = RALF_BASE + 0x200;

  const int BILLING_REQUEST_SENT = RALF_BASE + 0x300;

  const int INTERIM_TIMER_POPPED = RALF_BASE + 0x400;
  const int INTERIM_TIMER_CREATED = RALF_BASE + 0x500;
  const int INTERIM_TIMER_RENEWED = RALF_BASE + 0x600;

  const int INCOMING_REQUEST = RALF_BASE + 0x700;
  const int REQUEST_REJECTED = RALF_BASE + 0x800;

  const int CDF_FAILOVER = RALF_BASE + 0x900;
  const int BILLING_REQUEST_NOT_SENT = RALF_BASE + 0xA00;
  const int BILLING_REQUEST_REJECTED = RALF_BASE + 0xB00;
  const int BILLING_REQUEST_SUCCEEDED = RALF_BASE + 0xC00;

} // namespace SASEvent


#endif

