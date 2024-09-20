// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/***************************************************************************

    Sun bwtwo monochrome video controller on SBus

***************************************************************************/

#ifndef MAME_BUS_SBUS_BWTWO_H
#define MAME_BUS_SBUS_BWTWO_H

#pragma once

#include "sbus.h"
#include "sun_bwtwo.h"


class sbus_bwtwo_device : public sun_bwtwo_device, public device_sbus_card_interface
{
public:
	// construction/destruction
	sbus_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	// device_t overrides
	virtual const tiny_rom_entry *device_rom_region() const override;

	// device_sbus_slot_interface overrides
	virtual void install_device() override;

	virtual void mem_map(address_map &map) override;

	uint32_t rom_r(offs_t offset);

private:
	required_memory_region m_rom;
};


DECLARE_DEVICE_TYPE(SBUS_BWTWO, sbus_bwtwo_device)

#endif // MAME_BUS_SBUS_BWTWO_H
