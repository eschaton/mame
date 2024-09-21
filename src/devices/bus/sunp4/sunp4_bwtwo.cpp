// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/***************************************************************************

    Sun bwtwo monochrome video controller on SBus

***************************************************************************/

#include "sunp4_bwtwo.h"

#include "emu.h"
#include "screen.h"

#include <algorithm>


DEFINE_DEVICE_TYPE(SUNP4_BWTWO, sunp4_bwtwo_device, "sunp4_bwtwo", "Sun bwtwo P4 Video")

void sunp4_bwtwo_device::mem_map(address_map &map)
{
	map(0x00400000, 0x0040001f).rw(m_bwtwo, FUNC(sun_bwtwo_device::regs_r), FUNC(sun_bwtwo_device::regs_w));
	map(0x00800000, 0x008fffff).rw(m_bwtwo, FUNC(sun_bwtwo_device::vram_r), FUNC(sun_bwtwo_device::vram_w));
}

sunp4_bwtwo_device::sunp4_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SUNP4_BWTWO, tag, owner, clock)
	, device_sunp4_card_interface(mconfig, *this)
	, m_bwtwo(*this, "bwtwo")
{
}

void sunp4_bwtwo_device::install_device()
{
	m_sunp4->install_device(m_base, m_base + 0x1ffffff, *this, &sunp4_bwtwo_device::mem_map);
}

void sunp4_bwtwo_device::device_add_mconfig(machine_config &config)
{
	SUN_BWTWO(config, m_bwtwo, 0);
}

void sunp4_bwtwo_device::device_start()
{
}
