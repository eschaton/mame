// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

    Sun P4 Bus MG3 and MG4 bwtwo-based monochrome video controller

	- The MG3 is ECL-only, J5401 selects between 1152x900 and 1600x1280.
	- The MG4 is analog/ECL and only supports 1152x900.

***************************************************************************/

#ifndef MAME_BUS_SUNP4_MG_H
#define MAME_BUS_SUNP4_MG_H

#pragma once

#include "machine/sun_bwtwo.h"

#include "sunp4.h"


class sunp4_mg_device : public device_t, public device_sunp4_card_interface
{
public:
	// construction/destruction
	sunp4_mg_device(const machine_config &mconfig, const device_type &type, const char *tag, device_t *owner, uint32_t clock);

protected:
	// device_t overrides
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;

	// device_sunp4_slot_interface overrides
	virtual void install_device() override;

	virtual void mem_map(address_map &map) override;

private:
	required_device<sun_bwtwo_device> m_bwtwo;
};


// Sun P4-Bus MG3 ECL Video Card
class sunp4_mg3_device : public sunp4_mg_device
{
public:
	// construction/destruction
	sunp4_mg3_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};


// Sun P4-Bus MG4 Analog/ECL Video Card
class sunp4_mg4_device : public sunp4_mg_device
{
public:
	// construction/destruction
	sunp4_mg4_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};


DECLARE_DEVICE_TYPE(SUNP4_MG3, sunp4_mg3_device)
DECLARE_DEVICE_TYPE(SUNP4_MG4, sunp4_mg4_device)

#endif // MAME_BUS_SUNP4_MG_H
