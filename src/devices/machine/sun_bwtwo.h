// license:BSD-3-Clause
// copyright-holders:Ryan Holtz, Chris Hanson
/***************************************************************************

    Sun bwtwo monochrome video controller

    - TODO:
      - VRAM should be provided to the device
      - Screen should be provided to the device

***************************************************************************/

#ifndef DEVICE_MACHINE_SUN_BWTWO_H
#define DEVICE_MACHINE_SUN_BWTWO_H

#pragma once

#include "device.h"


/*!
 Resolutions supported by bwtwo implementations, sometimes specified via
 a jumper on the board containing the bwtwo, sometimes via monitor sense
 lines. The values are passed via bits 6-4 in the bwtwo status register.

 - Note: Not all bwtwo implementations support all resolutions.
 */
enum sun_bwtwo_resolution {
	sun_bwtwo_resolution_1024x768   = 0x10,
	sun_bwtwo_resolution_1152x900   = 0x30,
	sun_bwtwo_resolution_1280x1024  = 0x40,
	sun_bwtwo_resolution_1600x1280  = 0x50,
};


class sun_bwtwo_device : public device_t
{
public:
	// construction/destruction
	sun_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	uint8_t regs_r(offs_t offset);
	void regs_w(offs_t offset, uint8_t data);

	uint8_t vram_r(offs_t offset);
	void vram_w(offs_t offset, uint8_t data);

	// direct resolution read/write so it can be configured from a jumper or display choice
	enum sun_bwtwo_resolution resolution_r();
	void resolution_w(enum sun_bwtwo_resolution value);

protected:
	// device_t overrides
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;

private:
	// TODO: These don't belong in this device.
	void configure_screen(machine_config &config);
	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	void configure_mono_lut();

	// registers
	uint8_t m_control;
	uint8_t m_status;
	enum sun_bwtwo_resolution m_resolution;

	// TODO: These don't belong in this device.
	std::unique_ptr<uint8_t[]> m_vram;
	required_device<screen_device> m_screen;
	uint32_t m_mono_lut[256][8];
};


DECLARE_DEVICE_TYPE(SUN_BWTWO, sun_bwtwo_device)


#endif // DEVICE_MACHINE_SUN_BWTWO_H
