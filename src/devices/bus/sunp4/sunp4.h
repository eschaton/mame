// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/***************************************************************************

  sunp4.h - Sun P4 slot bus and card emulation

***************************************************************************/

#ifndef MAME_BUS_SUNP4_SUNP4_H
#define MAME_BUS_SUNP4_SUNP4_H

#pragma once

// TODO: Remove SPARC dependency.

#include "cpu/sparc/sparc.h"
#include "machine/bankdev.h"

class device_sunp4_card_interface;
class sunp4_device;


class sunp4_slot_device : public device_t, public device_single_card_slot_interface<device_sunp4_card_interface>
{
public:
	// construction/destruction
	template <typename T, typename U>
	sunp4_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&sunp4_tag, int slot, U &&opts, const char *dflt, bool fixed = false)
		: sunp4_slot_device(mconfig, tag, owner, clock)
	{
		option_reset();
		opts(*this);
		set_default_option(dflt);
		set_fixed(fixed);
		m_sunp4.set_tag(std::forward<T>(sunp4_tag));
		m_slot = slot;
	}
	sunp4_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	sunp4_slot_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	// device_t implementation
	virtual void device_validity_check(validity_checker &valid) const override;
	virtual void device_resolve_objects() override;
	virtual void device_start() override;

	// configuration
	required_device<sunp4_device> m_sunp4;
	int m_slot;
};

DECLARE_DEVICE_TYPE(SUNP4_SLOT, sunp4_slot_device)


class sunp4_device : public device_t,
	public device_memory_interface
{
	friend class device_sunp4_card_interface;
public:
	// construction/destruction
	template <typename T, typename U>
	sunp4_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&cpu_tag, U &&space_tag, int space_num)
		: sunp4_device(mconfig, tag, owner, clock)
	{
		set_cpu(std::forward<T>(cpu_tag));
		set_type1space(std::forward<U>(space_tag), space_num);
	}

	sunp4_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// inline configuration
	template <typename T> void set_cpu(T &&tag) { m_maincpu.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_type1space(T &&tag, int num) { m_type1space.set_tag(std::forward<T>(tag), num); }
	template <unsigned Line> auto irq() { return m_irq_cb[Line].bind(); }
	auto buserr() { return m_buserr.bind(); }

	virtual space_config_vector memory_space_config() const override;

	const address_space_config m_space_config;

	void add_sunp4_card(int slot, device_sunp4_card_interface *card);
	device_sunp4_card_interface *get_sunp4_card(int slot);

	void set_irq_line(int state, int line);

	template<typename T> void install_device(offs_t addrstart, offs_t addrend, T &device, void (T::*map)(class address_map &map), uint64_t unitmask = ~u64(0))
	{
		m_space->install_device(addrstart, addrend, device, map, unitmask);
	}

	uint32_t read(offs_t offset, uint32_t mem_mask = ~0);
	void write(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

protected:
	sunp4_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	// device_t implementation
	virtual void device_start() override;

	// internal state
	required_device<sparc_base_device> m_maincpu;
	required_address_space m_type1space;
	address_space *m_space;

	devcb_write_line::array<7> m_irq_cb;
	devcb_write32 m_buserr;

	device_sunp4_card_interface *m_device_list[3];

private:
	void slot1_timeout_map(address_map &map);
	void slot2_timeout_map(address_map &map);
	void slot3_timeout_map(address_map &map);

	template <unsigned Slot> uint32_t slot_timeout_r();
	template <unsigned Slot> void slot_timeout_w(uint32_t data);
};

DECLARE_DEVICE_TYPE(SUNP4, sunp4_device)


// class representing interface-specific live sunp4 card
class device_sunp4_card_interface : public device_interface
{
	friend class sunp4_device;
public:
	// construction/destruction
	virtual ~device_sunp4_card_interface();

	// inline configuration
	void set_sunp4(sunp4_device *sunp4, int slot);
	template <typename T> void set_onboard(T &&sunp4, int slot) { m_sunp4_finder.set_tag(std::forward<T>(sunp4)); m_slot = slot; }

	virtual void mem_map(address_map &map) = 0;

protected:
	void raise_irq(int line) { m_sunp4->set_irq_line(ASSERT_LINE, line); }
	void lower_irq(int line) { m_sunp4->set_irq_line(CLEAR_LINE, line); }

	device_sunp4_card_interface(const machine_config &mconfig, device_t &device);

	virtual void interface_validity_check(validity_checker &valid) const override;
	virtual void interface_pre_start() override;
	virtual void interface_post_start() override;
	virtual void install_device() = 0;

	sunp4_device &sunp4() { assert(m_sunp4); return *m_sunp4; }

	optional_device<sunp4_device> m_sunp4_finder;
	sunp4_device *m_sunp4;
	const char *m_sunp4_slottag;
	int m_slot;
	uint32_t m_base;
};

void sunp4_cards(device_slot_interface &device);

#endif  // MAME_BUS_SUNP4_SUNP4_H
