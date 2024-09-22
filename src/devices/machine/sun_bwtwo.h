// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/***************************************************************************

    Sun bwtwo monochrome video controller

***************************************************************************/

#ifndef DEVICE_MACHINE_SUN_BWTWO_H
#define DEVICE_MACHINE_SUN_BWTWO_H

#pragma once

#include "machine/bankdev.h"


// (Potentially) supported resolutions, usually specified via a jumper
// on the board containing the bwtwo.
enum sun_bwtwo_resolution {
	sun_bwtwo_resolution_normal,		// Sun 1152x900 resolution
	sun_bwtwo_resolution_highres,		// Sun 1600x1280 resolution
};


class sun_bwtwo_device : public device_t
{
public:
	// construction/destruction
	sun_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	uint8_t bwtwo_r(offs_t offset);
	void bwtwo_w(offs_t offset, uint8_t data);

	enum sun_bwtwo_resolution resolution_r();
	void resolution_w(enum sun_bwtwo_resolution value);

	uint8_t regs_r(offs_t offset);
	void regs_w(offs_t offset, uint8_t data);
	uint8_t vram_r(offs_t offset);
	void vram_w(offs_t offset, uint8_t data);

protected:
	// device_t overrides
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;

private:
	void configure_screen(machine_config &config);
	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	enum sun_bwtwo_resolution m_resolution;

	std::unique_ptr<uint8_t[]> m_vram;
	required_device<screen_device> m_screen;
	uint32_t m_mono_lut[256][8];
};


DECLARE_DEVICE_TYPE(SUN_BWTWO, sun_bwtwo_device)

#endif // DEVICE_MACHINE_SUN_BWTWO_H
