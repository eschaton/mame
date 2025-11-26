// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

    DEC PMAGB-BA Smart Frame Buffer (SFB) TURBOchannel Card

***************************************************************************/

#include "emu.h"

#include "pmagb_ba.h"

#include "video/bt459.h"
#include "video/decsfb.h"

#include "screen.h"

namespace {

ROM_START(pmagb_ba)
	ROM_REGION32_LE( 0x20000, "pmagb_ba", 0 )
	ROM_DEFAULT_BIOS("1.1")
	ROM_SYSTEM_BIOS(0, "1.1", "PMAGB_BA 1.1")
	ROMX_LOAD("pmagb-ba-rom.img", 0x000000, 0x020000, CRC(91f40ab0) SHA1(a39ce6ed52697a513f0fb2300a1a6cf9e2eabe33), ROM_BIOS(0))
ROM_END


class pmagb_ba_device : public device_t
					  , public device_tc_card_interface
{
public:
	pmagb_ba_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
		: device_t(mconfig, PMAGB_BA, tag, owner, clock)
		, device_tc_card_interface(mconfig, *this)
		, m_screen(*this, "screen")
		, m_sfb(*this, "sfb")
		, m_bt459(*this, "bt459")
		, m_vrom(*this, "pmagb_ba")
	{
	}

protected:
	// device_t implementation
	virtual void device_start() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

	virtual void install_device() override ATTR_COLD;

private:
	void mem_map(address_map &map) override ATTR_COLD;

	u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	u32 cfb_r(offs_t offset);
	void cfb_w(offs_t offset, u32 data, u32 mem_mask = ~0);

	required_device<screen_device> m_screen;
	required_device<decsfb_device> m_sfb;
	required_device<bt459_device> m_bt459;
	optional_memory_region m_vrom;

	u8 *m_vrom_ptr;
};



void pmagb_ba_device::device_start()
{
	if (m_vrom) {
		m_vrom_ptr = m_vrom->base();
	}
}

void pmagb_ba_device::device_add_mconfig(machine_config &config)
{
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_raw(130000000, 1704, 32, (1280+32), 1064, 3, (1024+3));
	m_screen->set_screen_update(FUNC(pmagb_ba_device::screen_update));

	DECSFB(config, m_sfb, 25'000'000);  // clock based on white paper which quotes "40ns" gate array cycle times
	m_sfb->int_cb().set(FUNC(device_tc_card_interface::int_w));

	BT459(config, m_bt459, 83'020'800);
}

const tiny_rom_entry *pmagb_ba_device::device_rom_region() const
{
	return ROM_NAME(pmagb_ba);
}

void pmagb_ba_device::install_device()
{
	m_bus->install_device(0x00000000, 0x007fffff, *this, &pmagb_ba_device::mem_map);
}

void pmagb_ba_device::mem_map(address_map &map)
{
	map(0x10000000, 0x1007ffff).rw(FUNC(pmagb_ba_device::cfb_r), FUNC(pmagb_ba_device::cfb_w));
	map(0x10100000, 0x101001ff).rw(m_sfb, FUNC(decsfb_device::read), FUNC(decsfb_device::write));
	map(0x101c0000, 0x101c000f).m("bt459", FUNC(bt459_device::map)).umask32(0x000000ff);
	map(0x10300000, 0x103fffff).rw(m_sfb, FUNC(decsfb_device::vram_r), FUNC(decsfb_device::vram_w));
}

u32 pmagb_ba_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	m_bt459->screen_update(screen, bitmap, cliprect, (uint8_t *)m_sfb->get_vram());
	return 0;
}

u32 pmagb_ba_device::cfb_r(offs_t offset)
{
	u32 const addr = offset << 2;

	//logerror("cfb_r: reading at %x\n", addr);

	if (addr < 0x80000)
	{
		return m_vrom_ptr[addr>>2] & 0xff;
	}

	if ((addr >= 0x100000) && (addr < 0x100200))
	{
	}

	if ((addr >= 0x200000) && (addr < 0x400000))
	{
	}

	return 0xffffffff;
}

void pmagb_ba_device::cfb_w(offs_t offset, u32 data, u32 mem_mask)
{
	u32 const addr = offset << 2;

	if ((addr >= 0x100000) && (addr < 0x100200))
	{
		return;
	}

	if ((addr >= 0x1c0000) && (addr < 0x200000))
	{
		//printf("Bt459: %08x (mask %08x) @ %x\n", data, mem_mask, offset<<2);
		return;
	}

	if ((addr >= 0x200000) && (addr < 0x400000))
	{
	}
}

} // anonymous namespace


DEFINE_DEVICE_TYPE_PRIVATE(PMAGB_BA, device_tc_card_interface, pmagb_ba_device, "pmagb_ba", "PMAGB_BA Smart Frame Buffer")
