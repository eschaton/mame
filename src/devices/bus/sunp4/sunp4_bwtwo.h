// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/***************************************************************************

    Sun bwtwo monochrome video controller on Sun P4 Bus

***************************************************************************/

#ifndef MAME_BUS_SUNP4_BWTWO_H
#define MAME_BUS_SUNP4_BWTWO_H

#pragma once

#include "machine/sun_bwtwo.h"

#include "sunp4.h"


class sunp4_bwtwo_device : public device_t, public device_sunp4_card_interface
{
public:
	// construction/destruction
	sunp4_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

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


DECLARE_DEVICE_TYPE(SUNP4_BWTWO, sunp4_bwtwo_device)

#endif // MAME_BUS_SUNP4_BWTWO_H
