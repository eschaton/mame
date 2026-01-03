// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/**********************************************************************

    DEC TURBOchannel emulation

**********************************************************************/

#ifndef MAME_BUS_TC_TC_H
#define MAME_BUS_TC_TC_H

#pragma once

#include <functional>
#include <vector>

class tc_device;
class tc_slot_device;

class device_tc_card_interface : public device_interface
{
	friend class tc_device;

public:
	// TURBOchannel interface

	void int_w(int state) { m_out_int_cb(state); }

protected:
	// construction/destruction
	device_tc_card_interface(const machine_config &mconfig, device_t &device);

	void install_device(tc_slot_device *slot) {
		m_slot = slot;
	}

	tc_device &tc() { return *m_bus; }
	tc_slot_device &slot() { return *m_slot; }

private:
	tc_device *m_bus;
	tc_slot_device *m_slot;

	// This is for tc_device's use: The slot itself has the interrupt
	// callback that is used by the system implementation, since the
	// mapping can be different on every system.
	auto int_cb() { return m_out_int_cb.bind(); }
	devcb_write_line m_out_int_cb;
};

class tc_device : public device_t
				, public device_memory_interface
{
public:
	// construction/destruction
	template <typename T>
	tc_device(const machine_config &mconfig, const char *tag, device_t *owner, T &&cputag, int spacenum)
		: tc_device(mconfig, tag, owner, (u32)0)
	{
		set_cputag(std::forward<T>(cputag), spacenum);
	}

	tc_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	~tc_device();

	// inline configuration
	template <typename T> void set_space(T &&tag, int spacenum) { m_space.set_tag(std::forward<T>(tag), spacenum); }
	void set_view(memory_view::memory_view_entry &view) { m_view = &view; };

	virtual space_config_vector memory_space_config() const override;
	address_space &program_space() const { return *m_space; }

	void add_card(device_tc_card_interface &card, tc_slot_device &slot);

	template<typename T>
	void install_device(offs_t addrstart, offs_t addrend, T &device, void (T::*map)(class address_map &map), u32 unitmask = ~u32(0))
	{
		m_space->install_device(addrstart, addrend, device, map, unitmask);
	}

	template <typename T>
	void install_map(T &device, void (T::*map)(address_map &map));

	const address_space_config m_program_config;

protected:
	// device_t implementation
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// internal state
	required_address_space m_space;
	memory_view::memory_view_entry *m_view;

private:
	using card_vector = std::vector<std::reference_wrapper<device_tc_card_interface>>;

	card_vector m_device_list;
};

class tc_slot_device : public device_t
					 , public device_slot_interface
{
public:
	// construction/destruction
	template <typename T>
	tc_slot_device(machine_config const &mconfig, char const *tag, device_t *owner, T &&opts, char const *dflt)
		: tc_slot_device(mconfig, tag, owner, DERIVED_CLOCK(1, 1))
	{
		option_reset();
		opts(*this);
		set_default_option(dflt);
		set_fixed(false);
	}
	tc_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	auto int_cb() { return m_out_int_cb.bind(); }

	void int_w(int state) { m_out_int_cb(state); }

	offs_t get_addrstart() const { return m_addrstart; }
	offs_t get_addrend() const { return m_addrend; }

	void set_address_range(offs_t addrstart, offs_t addrend) {
		m_addrstart = addrstart;
		m_addrend = addrend;
	}

protected:
	// device_t implementation
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	device_tc_card_interface *m_card;
	offs_t m_addrstart, m_addrend;

private:
	required_device<tc_device> m_bus;

	devcb_write_line m_out_int_cb;
};


template <typename T>
void tc_device::install_map(T &device, void (T::*map)(address_map &map))
{
	device_tc_card_interface *dev = dynamic_cast<device_tc_card_interface *>(&device);
	if (dev) {
		tc_slot_device &slot = dev->slot();
		const offs_t start = slot.get_addrstart();
		const offs_t end = slot.get_addrend();

		space(AS_PROGRAM).install_device(start, end, device, map);
	}
}


DECLARE_DEVICE_TYPE(TC, tc_device)
DECLARE_DEVICE_TYPE(TC_SLOT, tc_slot_device)

void tc_cards(device_slot_interface &device);

#endif // MAME_BUS_TC_TC_H
