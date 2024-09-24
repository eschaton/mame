// license:BSD-3-Clause
// copyright-holders:Ryan Holtz, Chris Hanson
/***************************************************************************

    Sun bwtwo monochrome video controller

***************************************************************************/

#include "emu.h"
#include "screen.h"
#include "sun_bwtwo.h"


DEFINE_DEVICE_TYPE(SUN_BWTWO, sun_bwtwo_device, "bwtwo", "Sun bwtwo Video")

sun_bwtwo_device::sun_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SUN_BWTWO, tag, owner, clock)
	, m_screen(*this, "screen")
{
}

uint8_t sun_bwtwo_device::regs_r(offs_t offset)
{
	logerror("%s: regs_r (unimplemented): %08x\n", machine().describe_context(), 0x400000 + offset);
	return 0;
}

void sun_bwtwo_device::regs_w(offs_t offset, uint8_t data)
{
	logerror("%s: regs_w (unimplemented): %08x = %02x\n", machine().describe_context(), 0x400000 + offset, data);
}

uint8_t sun_bwtwo_device::vram_r(offs_t offset)
{
	return m_vram[offset];
}

void sun_bwtwo_device::vram_w(offs_t offset, uint8_t data)
{
	m_vram[offset] = data;
}

void sun_bwtwo_device::device_add_mconfig(machine_config &config)
{
	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_RASTER));
	screen.set_screen_update(FUNC(sun_bwtwo_device::screen_update));
	screen.set_size(1152, 900);
	screen.set_visarea(0, 1152-1, 0, 900-1);
	screen.set_refresh_hz(72);
}

void sun_bwtwo_device::device_start()
{
	m_vram = std::make_unique<uint8_t[]>(0x100000);
	save_pointer(NAME(m_vram), 0x100000);

	for (int i = 0; i < 0x100; i++)
	{
		for (int bit = 7; bit >= 0; bit--)
		{
			m_mono_lut[i][7 - bit] = BIT(i, bit) ? 0 : ~0;
		}
	}
}

uint32_t sun_bwtwo_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint8_t const *line = &m_vram[0];

	for (int y = 0; y < 900; y++)
	{
		uint32_t *scanline = &bitmap.pix(y);
		for (int x = 0; x < 1152/8; x++)
		{
			std::copy_n(m_mono_lut[*line], 8, scanline);
			line++;
			scanline += 8;
		}
	}

	return 0;
}
