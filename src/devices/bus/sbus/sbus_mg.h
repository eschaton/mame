// license:BSD-3-Clause
// copyright-holders:Ryan Holtz, Chris Hanson
/***************************************************************************

    Sun MG1 & MG2 monochrome video controller on SBus
    - Both use the bwtwo video controller
    - MG1 supports only ECL digital video
    - MG2 supports ECL digital or analog video, with display type sense

***************************************************************************/

#ifndef MAME_BUS_SBUS_MG_H
#define MAME_BUS_SBUS_MG_H

#pragma once

#include "sbus.h"
#include "video/sun_bwtwo.h"


class sbus_bwtwo_device : public device_t
						, public device_sbus_card_interface
{
public:
	// construction/destruction
	sbus_bwtwo_device(const machine_config &mconfig, const device_type &type, const char *tag, device_t *owner, uint32_t clock);

protected:
	// device_t overrides
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual const tiny_rom_entry *device_rom_region() const override;

	// device_sbus_slot_interface overrides
	virtual void mem_map(address_map &map) override;
	virtual void install_device() override;

	uint32_t rom_r(offs_t offset);

private:
	required_device<sun_bwtwo_device> m_bwtwo;
	required_memory_region m_rom;
};


class sbus_mg1_device : public sbus_bwtwo_device
{
public:
	// construction/destruction
	sbus_mg1_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};


class sbus_mg2_device : public sbus_bwtwo_device
{
public:
	// construction/destruction
	sbus_mg2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};


DECLARE_DEVICE_TYPE(SBUS_MG1, sbus_mg1_device)
DECLARE_DEVICE_TYPE(SBUS_MG2, sbus_mg2_device)

#endif // MAME_BUS_SBUS_MG_H
