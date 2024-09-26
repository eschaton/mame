// license:BSD-3-Clause
// copyright-holders:Ryan Holtz, Chris Hanson
/***************************************************************************

    Sun bwtwo monochrome video controller

    The BW2 was implemented differently in different systems, but provided
    the same interface across them. Some implementations used dedicated VRAM
    while others, such as on the Sun-3/50, used a portion of system RAM.

    The CG3 shared implementation with the BW2; in particular, the CG3's
    register set is a superset of the BW2's, and the CG3's overlay plane is
    essentially a colocated BW2.

***************************************************************************/

#include "emu.h"
#include "screen.h"
#include "sun_bwtwo.h"

#define LOG_REGISTER        (1U << 2)
#define LOG_OUTPUT_FUNC     printf
// #define  VERBOSE             0
#define VERBOSE             (LOG_GENERAL | LOG_REGISTER)

#include "logmacro.h"

#define LOGREGISTER(...)    LOGMASKED(LOG_REGISTER, __VA_ARGS__)


DEFINE_DEVICE_TYPE(SUN_BWTWO, sun_bwtwo_device, "bwtwo", "Sun bwtwo Video")


sun_bwtwo_device::sun_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SUN_BWTWO, tag, owner, clock)
	, device_memory_interface(mconfig, *this)
	, device_video_interface(mconfig, *this, false)
	, m_space_config("vram", ENDIANNESS_BIG, 8, 20, 0, address_map_constructor(FUNC(sun_bwtwo_device::sun_bwtwo_vram), this))
	, m_control(0)
	, m_interrupts_enabled(false)
	, m_video_enabled(false)
{
}

#define	BW2_REG_CONTROL	0x10
#define BW2_REG_STATUS	0x11

uint8_t sun_bwtwo_device::regs_r(offs_t offset)
{
	uint8_t data;

	switch (offset) {
		case BW2_REG_CONTROL:
			data = m_control;
			LOGREGISTER("sun_bwtwo: control_r: 0x%02x\n", data);
			break;

		case BW2_REG_STATUS:
			data = status_r();
			break;

		default:
			LOGREGISTER("sun_bwtwo: regs_r (unimplemented): %08x\n", offset);
			data = 0;
			break;
	}

	return data;
}

void sun_bwtwo_device::regs_w(offs_t offset, uint8_t data)
{
	switch (offset) {
		case BW2_REG_CONTROL:
			control_w(data);
			break;

		case BW2_REG_STATUS:
			LOGREGISTER("sun_bwtwo: status_w (unsupported): 0x%02x\n", data);
			// Ignore writes to the status register.
			break;

		default:
			LOGREGISTER("sun_bwtwo: regs_w (unimplemented): %08x = %02x\n", offset, data);
			break;
	}
}

uint8_t sun_bwtwo_device::vram_r(offs_t offset)
{
	return vram_read(offset);
}

void sun_bwtwo_device::vram_w(offs_t offset, uint8_t data)
{
	vram_write(offset, data);
}

void sun_bwtwo_device::device_add_mconfig(machine_config &config)
{
}

void sun_bwtwo_device::device_start()
{
}

device_memory_interface::space_config_vector sun_bwtwo_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(0, &m_space_config)
	};
}

uint32_t sun_bwtwo_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	if (m_video_enabled && has_screen()) {
		int const width = screen.width();
		int const height = screen.height();
		uint32_t const black = 0x000000;
		uint32_t const white = 0xffffff;

		for (int y = 0; y < height; y++) {
			uint32_t *scanline = &bitmap.pix(y);
			for (int x = 0; x < width/8; x++) {
				uint8_t const pixels = vram_read((y * width/8) + x);

				*scanline++ = BIT(pixels, 7) ? black : white;
				*scanline++ = BIT(pixels, 6) ? black : white;
				*scanline++ = BIT(pixels, 5) ? black : white;
				*scanline++ = BIT(pixels, 4) ? black : white;
				*scanline++ = BIT(pixels, 3) ? black : white;
				*scanline++ = BIT(pixels, 2) ? black : white;
				*scanline++ = BIT(pixels, 1) ? black : white;
				*scanline++ = BIT(pixels, 0) ? black : white;
			}
		}
	} else {
		// If there's no screen, or video is disabled even though there's a
		// screen, just indicate that there's no change to the bitmap.
		return UPDATE_HAS_NOT_CHANGED;
	}

	return 0;
}

void sun_bwtwo_device::control_w(uint8_t data)
{
	LOGREGISTER("sun_bwtwo: control_w: 0x%02x\n", data);

	m_control = data;

	// Bit 7 enables interrupts.
	m_interrupts_enabled = BIT(m_control, 7);
	LOGREGISTER("sun_bwtwo: control_w: interrupts enabled: %s\n", m_interrupts_enabled ? "true" : "false");

	// Bit 6 enables video output, which appears to latch on enable.
	m_video_enabled = BIT(m_control, 6);
	LOGREGISTER("sun_bwtwo: control_w: video enabled: %s\n", m_video_enabled ? "true" : "false");

	// Bit 5 enables master timing.
	// Bit 4 enables cursor compare.
	// Bits 3..2 select the clock crystal to use.
	// Bits 1..0 select the clock divisor to use.
	// TODO: Implement these.
}

uint8_t sun_bwtwo_device::status_r()
{
	uint8_t status = 0;

	// Status register layout determined by examining the NetBSD bwtwo driver.

	// Bit 7 of the status register indicates whether an interrupt is pending.
	bool interrupt_pending = false; // TODO: Implement this.
	status |= (interrupt_pending ? 1 : 0) << 7;
	LOGREGISTER("sun_bwtwo: status_r: interrupt pending = %s\n", interrupt_pending ? "true" : "false");

	// Bits 6..4 of the status register specify the monitor sense code.
	// Derive that from the width of the attached screen, if any, otherwise
	// assume a default size.
	const int width = has_screen() ? screen().width() : 1152;
	uint8_t monsense;
	switch (width) {
		case 1024: monsense = 0x1; break;
		case 1152: monsense = 0x3; break;
		case 1280: monsense = 0x4; break;
		case 1600: monsense = 0x5; break;
		default: {
			monsense = 0x0;
		} break;
	}
	status |= (monsense << 4);
	LOGREGISTER("sun_bwtwo: status_r: monitor sense = 0x%02x\n", monsense);

	// Bits 3..0 of the status register define the monitor ID. Known IDs are:
	// - 0x1:	Color
	// - 0x2:	Analog monochrome
	// - 0x3:	ECL monochrome
	uint8_t monid = 0x3; // always assume ECL monochrome for now
	status |= monid;
	LOGREGISTER("sun_bwtwo: status_r: monitor ID = 0x%02x\n", monid);

	return status;
}

void sun_bwtwo_device::sun_bwtwo_vram(address_map &map)
{
	if (!has_configured_map()) {
		map(0x00000000, 0x000fffff).ram();
	}
}

inline uint8_t sun_bwtwo_device::vram_read(offs_t address)
{
	return space().read_byte(address);
}

inline void sun_bwtwo_device::vram_write(offs_t address, uint8_t data)
{
	space().write_byte(address, data);
}
