// license:BSD-3-Clause
// copyright-holders:Ryan Holtz, Chris Hanson
/***************************************************************************

    Sun bwtwo monochrome video controller

***************************************************************************/

#ifndef MAME_DEVICE_VIDEO_SUN_BWTWO_H
#define MAME_DEVICE_VIDEO_SUN_BWTWO_H

#pragma once

//#include "cpu/sparc/sparc.h"
//#include "machine/bankdev.h"


class sun_bwtwo_device : public device_t
					   , public device_memory_interface
					   , public device_video_interface
{
public:
	// construction/destruction
	sun_bwtwo_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// register access
	uint8_t regs_r(offs_t offset);
	void regs_w(offs_t offset, uint8_t data);

	// VRAM access
	uint8_t vram_r(offs_t offset);
	void vram_w(offs_t offset, uint8_t data);

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

protected:
	// device_t overrides
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;

	// device_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;

private:
	void control_w(uint8_t data);
	uint8_t status_r();

	void sun_bwtwo_vram(address_map &map);
	inline uint8_t vram_read(offs_t address);
	inline void vram_write(offs_t address, uint8_t data);

	const address_space_config m_space_config;

	uint8_t m_control;
	bool m_interrupts_enabled;
	bool m_video_enabled;
};


DECLARE_DEVICE_TYPE(SUN_BWTWO, sun_bwtwo_device)

#endif // MAME_DEVICE_VIDEO_SUN_BWTWO_H
