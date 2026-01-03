// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

    DEC PMAGB-BA Smart Frame Buffer (SFB) TURBOchannel Card

    Derived from R. Belmont's kn02 implementation.

***************************************************************************/

#include "emu.h"

#include "pmagb_ba.h"

#include "video/bt459.h"
#include "video/decsfb.h"

#include "screen.h"

#define LOG_REG         (1U << 1)
#define LOG_IRQ         (1U << 2)

#define VERBOSE         (LOG_GENERAL|LOG_REG|LOG_IRQ)
#define LOG_OUTPUT_FUNC printf

#include "logmacro.h"


namespace {

ROM_START(pmagb_ba)
	ROM_REGION32_LE(0x20000, "pmagb_ba", 0)
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
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

	void mem_map(address_map &map);

private:
	u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	u32 vrom_r(offs_t offset);

	required_device<screen_device> m_screen;
	required_device<decsfb_device> m_sfb;
	required_device<bt459_device> m_bt459;
	optional_memory_region m_vrom;

	u8 *m_vrom_ptr;
};


void pmagb_ba_device::device_start()
{
	LOG("PMAGB_BA: Started\n");

	if (m_vrom) {
		m_vrom_ptr = m_vrom->base();
	}

	tc().install_map(*this, &pmagb_ba_device::mem_map);
}

void pmagb_ba_device::device_reset()
{
	LOG("PMAGB_BA: Reset\n");
}

void pmagb_ba_device::device_add_mconfig(machine_config &config)
{
	LOG("PMAGB_BA: Adding machine configuration\n");

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

void pmagb_ba_device::mem_map(address_map &map)
{
	LOG("PMAGB_BA: Installing address map\n");

	map(0x00000000, 0x0007ffff).r(FUNC(pmagb_ba_device::vrom_r));
	map(0x00100000, 0x001001ff).rw(m_sfb, FUNC(decsfb_device::read), FUNC(decsfb_device::write));
	map(0x001c0000, 0x001c000f).m("bt459", FUNC(bt459_device::map)).umask32(0x000000ff);
	map(0x00300000, 0x003fffff).rw(m_sfb, FUNC(decsfb_device::vram_r), FUNC(decsfb_device::vram_w));
}

u32 pmagb_ba_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	m_bt459->screen_update(screen, bitmap, cliprect, (uint8_t *)m_sfb->get_vram());
	return 0;
}

u32 pmagb_ba_device::vrom_r(offs_t offset)
{
	LOGMASKED(LOG_REG, "PMAGB_BA: vrom_r(offset: 0x%08x)\n", offset);

	return m_vrom_ptr[offset >> 2] & 0x000000ff;
}

} // anonymous namespace


DEFINE_DEVICE_TYPE_PRIVATE(PMAGB_BA, device_tc_card_interface, pmagb_ba_device, "pmagb_ba", "DEC PMAGB-BA Smart Frame Buffer")
