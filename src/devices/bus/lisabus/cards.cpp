// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

  Apple Lisa Expansion Bus Cards

***************************************************************************/

#include "cards.h"

#include "lisadualpp.h"

void lisabus_cards(device_slot_interface &device)
{
	device.option_add("lisadualpp", LISADUALPP);    // Apple Lisa Dual Parallel Port Card
}
