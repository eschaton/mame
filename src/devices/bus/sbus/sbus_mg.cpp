// license:BSD-3-Clause
// copyright-holders:Ryan Holtz, Chris Hanson
/***************************************************************************

    Sun bwtwo monochrome video controller on SBus

***************************************************************************/

#include "sbus_mg.h"

#include "emu.h"
#include "screen.h"


DEFINE_DEVICE_TYPE(SBUS_MG1, sbus_mg1_device, "sbus_mg1", "Sun MG1 SBus Video")
DEFINE_DEVICE_TYPE(SBUS_MG2, sbus_mg2_device, "sbus_mg2", "Sun MG2 SBus Video")


sbus_bwtwo_device::sbus_bwtwo_device(const machine_config &mconfig, const device_type &type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, device_sbus_card_interface(mconfig, *this)
	, m_bwtwo(*this, "bwtwo")
	, m_rom(*this, "prom")
{
}

void sbus_bwtwo_device::device_add_mconfig(machine_config &config)
{
	SUN_BWTWO(config, m_bwtwo, 0);
}

void sbus_bwtwo_device::device_start()
{
}

ROM_START( sbus_bwtwo )
	ROM_REGION32_BE(0x8000, "prom", ROMREGION_ERASEFF)

	ROM_SYSTEM_BIOS(0, "1081", "P/N 525-1081-01")
	ROMX_LOAD( "bw2_525-1081-01.bin", 0x0000, 0x8000, CRC(8b70c8c7) SHA1(fd750ad2fd6efdde957f8b0f9abf962e14fe221a), ROM_BIOS(0) )
	ROM_SYSTEM_BIOS(1, "1124", "P/N 525-1124-01")
	ROMX_LOAD( "bw2_525-1124-01.bin", 0x0000, 0x0800, CRC(e37a3314) SHA1(78761bd2369cb0c58ef1344c697a47d3a659d4bc), ROM_BIOS(1) )
ROM_END

const tiny_rom_entry *sbus_bwtwo_device::device_rom_region() const
{
	return ROM_NAME( sbus_bwtwo );
}

void sbus_bwtwo_device::install_device()
{
	m_sbus->install_device(m_base, m_base + 0x1ffffff, *this, &sbus_bwtwo_device::mem_map);
}

void sbus_bwtwo_device::mem_map(address_map &map)
{
	map(0x00000000, 0x00007fff).r(FUNC(sbus_bwtwo_device::rom_r));
	map(0x00400000, 0x0040001f).rw(m_bwtwo, FUNC(sun_bwtwo_device::regs_r), FUNC(sun_bwtwo_device::regs_w));
	map(0x00800000, 0x008fffff).rw(m_bwtwo, FUNC(sun_bwtwo_device::vram_r), FUNC(sun_bwtwo_device::vram_w));
}

uint32_t sbus_bwtwo_device::rom_r(offs_t offset)
{
	return ((uint32_t*)m_rom->base())[offset];
}


sbus_mg1_device::sbus_mg1_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: sbus_bwtwo_device(mconfig, SBUS_MG1, tag, owner, clock)
{
}


sbus_mg2_device::sbus_mg2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: sbus_bwtwo_device(mconfig, SBUS_MG2, tag, owner, clock)
{
}
