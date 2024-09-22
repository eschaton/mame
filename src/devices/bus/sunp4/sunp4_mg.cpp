// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

    Sun P4 Bus MG3 and MG4 bwtwo-based monochrome video controller

	- The MG3 is ECL-only, J5401 selects between 1152x900 and 1600x1280.
	- The MG4 is analog/ECL and only supports 1152x900.

***************************************************************************/

#include "sunp4_mg.h"

#include "emu.h"
#include "screen.h"

#include <algorithm>


DEFINE_DEVICE_TYPE(SUNP4_MG3, sunp4_mg3_device, "sunp4_mg3", "Sun P4-bus MG3 Video")
DEFINE_DEVICE_TYPE(SUNP4_MG4, sunp4_mg4_device, "sunp4_mg4", "Sun P4-bus MG4 Video")

sunp4_mg_device::sunp4_mg_device(const machine_config &mconfig, const device_type &type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, device_sunp4_card_interface(mconfig, *this)
	, m_bwtwo(*this, "bwtwo")
{
}

void sunp4_mg_device::device_add_mconfig(machine_config &config)
{
	SUN_BWTWO(config, m_bwtwo, 0);
}

void sunp4_mg_device::device_start()
{
}

void sunp4_mg_device::mem_map(address_map &map)
{
	map(0x00400000, 0x0040001f).rw(m_bwtwo, FUNC(sun_bwtwo_device::regs_r), FUNC(sun_bwtwo_device::regs_w));
	map(0x00800000, 0x008fffff).rw(m_bwtwo, FUNC(sun_bwtwo_device::vram_r), FUNC(sun_bwtwo_device::vram_w));
}

void sunp4_mg_device::install_device()
{
	m_sunp4->install_device(m_base, m_base + 0x1ffffff, *this, &sunp4_mg_device::mem_map);
}


sunp4_mg3_device::sunp4_mg3_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: sunp4_mg_device(mconfig, SUNP4_MG3, tag, owner, clock)
{
}


sunp4_mg4_device::sunp4_mg4_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: sunp4_mg_device(mconfig, SUNP4_MG3, tag, owner, clock)
{
}

