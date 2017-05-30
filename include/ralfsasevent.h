/**
 * @file ralfsasevent.h Ralf SAS event IDs
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef RALFSASEVENT_H__
#define RALFSASEVENT_H__

#include "sasevent.h"

namespace SASEvent
{
  //----------------------------------------------------------------------------
  // Ralf events
  //----------------------------------------------------------------------------

  const int NEW_RF_SESSION_OK = RALF_BASE + 0x000;
  const int NEW_RF_SESSION_ERR = RALF_BASE + 0x001;
  const int CONTINUED_RF_SESSION_OK = RALF_BASE + 0x100;
  const int CONTINUED_RF_SESSION_ERR = RALF_BASE + 0x101;
  const int END_RF_SESSION_OK = RALF_BASE + 0x200;
  const int END_RF_SESSION_ERR = RALF_BASE + 0x201;

  const int BILLING_REQUEST_SENT = RALF_BASE + 0x300;

  const int INTERIM_TIMER_POPPED = RALF_BASE + 0x400;
  const int INTERIM_TIMER_CREATED = RALF_BASE + 0x500;
  const int INTERIM_TIMER_RENEWED = RALF_BASE + 0x600;

  const int INCOMING_REQUEST = RALF_BASE + 0x700;
  const int INCOMING_REQUEST_NO_PEERS = RALF_BASE + 0x710;
  const int REQUEST_REJECTED_INVALID_JSON = RALF_BASE + 0x800;

  const int CDF_FAILOVER = RALF_BASE + 0x900;
  const int BILLING_REQUEST_NOT_SENT = RALF_BASE + 0xA00;
  const int BILLING_REQUEST_REJECTED = RALF_BASE + 0xB00;
  const int BILLING_REQUEST_SUCCEEDED = RALF_BASE + 0xC00;

  const int SESSION_DESERIALIZATION_FAILED = RALF_BASE + 0xD00;

} // namespace SASEvent


#endif
