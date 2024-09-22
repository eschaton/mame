// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/***************************************************************************

    Sun bwtwo monochrome video controller

***************************************************************************/

#include "emu.h"
#include "screen.h"
#include "sun_bwtwo.h"

#include <algorithm>

#define VERBOSE 1
// #define LOG_OUTPUT_FUNC osd_printf_info
#define	LOG_OUTPUT_FUNC	printf
#include "logmacro.h"

DEFINE_DEVICE_TYPE(SUN_BWTWO, sun_bwtwo_device, "bwtwo", "Sun BW2 Video")

sun_bwtwo_device::sun_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SUN_BWTWO, tag, owner, clock)
	, m_resolution(sun_bwtwo_resolution_normal)
	, m_screen(*this, "screen")
{
}

void sun_bwtwo_device::device_add_mconfig(machine_config &config)
{
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	configure_screen(config);
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

void sun_bwtwo_device::configure_screen(machine_config &config)
{
	uint16_t height, width;

	switch (m_resolution) {
		case sun_bwtwo_resolution_normal:
			width = 1152;
			height = 900;
			break;

		case sun_bwtwo_resolution_highres:
			width = 1600;
			height = 1280;
			break;
	}

	m_screen->set_screen_update(FUNC(sun_bwtwo_device::screen_update));
	m_screen->set_size(width, height);
	m_screen->set_visarea(0, width-1, 0, height-1);
	m_screen->set_refresh_hz(67);
}

uint32_t sun_bwtwo_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint16_t height, width;

	switch (m_resolution) {
		case sun_bwtwo_resolution_normal:
			width = 1152;
			height = 900;
			break;

		case sun_bwtwo_resolution_highres:
			width = 1600;
			height = 1280;
			break;
	}

	uint8_t const *line = &m_vram[0];

	for (int y = 0; y < height; y++)
	{
		uint32_t *scanline = &bitmap.pix(y);
		for (int x = 0; x < width/8; x++)
		{
			std::copy_n(m_mono_lut[*line], 8, scanline);
			line++;
			scanline += 8;
		}
	}

	return 0;
}

// From NetBSD
#define	BWREG_ID	0 + 0x00100000
#define	BWREG_REG	0x400000 + 0x00100000
#define	BWREG_MEM	0x800000 + 0x00100000

uint8_t sun_bwtwo_device::bwtwo_r(offs_t offset)
{
	uint8_t data;

	if (offset >= BWREG_MEM) {
		data = vram_r(offset - BWREG_MEM);
	} else if (offset >= BWREG_REG) {
		data = regs_r(offset - BWREG_REG);
	} else {
		data = 0;
	}

	LOG("%s: bwtwo_r: %08x = %02x\n", "bwtwo", offset, data);

	return data;
}

void sun_bwtwo_device::bwtwo_w(offs_t offset, uint8_t data)
{
	LOG("%s: bwtwo_w: %08x = %02x\n", "bwtwo", offset, data);

	if (offset >= BWREG_MEM) {
		vram_w(offset - BWREG_MEM, data);
	} else if (offset >= BWREG_REG) {
		return regs_w(offset - BWREG_REG, data);
	} else {
		// Do nothing.
	}
}

enum sun_bwtwo_resolution sun_bwtwo_device::resolution_r()
{
	return m_resolution;
}

void sun_bwtwo_device::resolution_w(enum sun_bwtwo_resolution value)
{
	if (m_resolution != value) {
		m_resolution = value;

		// TODO: Recreate the screen.
	}
}

/*
NetBSD:
struct fbcontrol {
	struct bt_regs {
		u_int	bt_addr;		// map address register				// 0
		u_int	bt_cmap;		// colormap data register			// 4
		u_int	bt_ctrl;		// control register					// 8
		u_int	bt_omap;		// overlay (cursor) map register	// C
	} fba_dac;
	u_char	fbc_ctrl;												// 10
	u_char	fbc_status;												// 11
	u_char	fbc_cursor_start;										// 12
	u_char	fbc_cursor_end;											// 13
	u_char	fbc_vcontrol[12];	// 12 bytes of video timing goo		// 14..1f
};
// fbc_ctrl bits:
#define FBC_IENAB	0x80		// Interrupt enable
#define FBC_VENAB	0x40		// Video enable
#define FBC_TIMING	0x20		// Master timing enable
#define FBC_CURSOR	0x10		// Cursor compare enable
#define FBC_XTALMSK	0x0c		// Xtal select (0,1,2,test)
#define FBC_DIVMSK	0x03		// Divisor (1,2,3,4)

// fbc_status bits:
#define FBS_INTR	0x80		// Interrupt pending
#define FBS_MSENSE	0x70		// Monitor sense mask
#define		FBS_1024X768	0x10
#define		FBS_1152X900	0x30
#define		FBS_1280X1024	0x40
#define		FBS_1600X1280	0x50
#define FBS_ID_MASK	0x0f		// ID mask
#define		FBS_ID_COLOR	0x01
#define		FBS_ID_MONO	0x02
#define		FBS_ID_MONO_ECL	0x03	// ?
*/

static const char * const reg_name(offs_t offset)
{
	switch (offset) {
		case 0x00: case 0x01: case 0x02: case 0x03:	return "fba_dac.bt_addr";
		case 0x04: case 0x05: case 0x06: case 0x07:	return "fba_dac.bt_cmap";
		case 0x08: case 0x09: case 0x0A: case 0x0B:	return "fba_dac.bt_ctrl";
		case 0x0C: case 0x0D: case 0x0E: case 0x0F:	return "fba_dac.bt_omap";
		case 0x10: return "fbc_ctrl";
		case 0x11: return "fbc_status";
		case 0x12: return "fbc_cursor_start";
		case 0x13: return "fbc_cursor_end";
		case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1A: case 0x1B:
		case 0x1C: case 0x1D: case 0x1E: case 0x1F:
			return "fbc_vcontrol";
		default: return "unknown";
	}
}

uint8_t sun_bwtwo_device::regs_r(offs_t offset)
{
	LOG("%s: regs_r (unimplemented): %s: %08x\n", "bwtwo", reg_name(offset), offset);
	return 0;
}

void sun_bwtwo_device::regs_w(offs_t offset, uint8_t data)
{
	LOG("%s: regs_w (unimplemented): %s: %08x = %02x\n", "bwtwo", reg_name(offset), offset, data);
}

uint8_t sun_bwtwo_device::vram_r(offs_t offset)
{
	return m_vram[offset];
}

void sun_bwtwo_device::vram_w(offs_t offset, uint8_t data)
{
	m_vram[offset] = data;
}
