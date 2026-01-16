// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

  Apple Lisa expansion slot bus

  Based on Apple II bus implementation by R. Belmont.

***************************************************************************/

#ifndef MAME_BUS_LISABUS_LISABUS_H
#define MAME_BUS_LISABUS_LISABUS_H

#pragma once

class lisabus_device;
class device_lisabus_card_interface;

class lisabus_slot_device: public device_t, public device_single_card_slot_interface<device_lisabus_card_interface>
{
public:
	template <typename T, typename U>
	lisabus_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&lisabus_tag, U &&opts, const char *dflt)
		: lisabus_slot_device(mconfig, tag, owner, clock)
	{
		option_reset();
		opts(*this);
		set_default_option(dflt);
		set_fixed(false);
		m_lisabus.set_tag(std::forward<T>(lisabus_tag));
	}

	lisabus_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// interface between bus and slot goes here

protected:
	virtual void device_resolve_objects() override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	required_device<lisabus_device> m_lisabus;
};

DECLARE_DEVICE_TYPE(LISABUS_SLOT, lisabus_slot_device)


class lisabus_device : public device_t
{
public:
	lisabus_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void add_lisabus_card(int slot, device_lisabus_card_interface *card);
	device_lisabus_card_interface *get_lisabus_card(int slot);

	// interface between card and bus

	void slot_int_w(int slot, int level) { if (!m_slot_int_w_cb.isunset()) m_slot_int_w_cb(slot, level); }
	template <std::size_t Slot> auto slot_iack_w_cb() { return m_slot_iack_w_cbs[Slot].bind(); }

	// interface between host and bus

	u16 bus_r(offs_t offset, u16 mask = ~0);
	void bus_w(offs_t offset, u16 data, u16 mask = ~0);

	auto slot_int_w_cb() { return m_slot_int_w_cb.bind(); }
	void slot_iack_w(int slot, int level) {
		if (!m_slot_iack_w_cbs[slot].isunset()) {
			m_slot_iack_w_cbs[slot](level);
		}
	}

	auto slot_berr_w_cb() { return m_slot_berr_w_cb.bind(); }
	void slot_berr_w(int level) { if (!m_slot_berr_w_cb.isunset()) m_slot_berr_w_cb(level); }

protected:
	lisabus_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
		: device_t(mconfig, type, tag, owner, clock)
		, m_slot_int_w_cb(*this)
		, m_slot_iack_w_cbs(*this)
		, m_slot_berr_w_cb(*this)
	{}

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	u16 slot_r(int slot, offs_t offset, u16 mask = ~0);
	void slot_w(int slot, offs_t offset, u16 data, u16 mask = ~0);

	device_lisabus_card_interface *m_device_list[3];
	devcb_write<int> m_slot_int_w_cb;
	devcb_write_line::array<3> m_slot_iack_w_cbs;
	devcb_write_line m_slot_berr_w_cb;
};

DECLARE_DEVICE_TYPE(LISABUS, lisabus_device)


class device_lisabus_card_interface : public device_interface
{
	friend class lisabus_device;

public:
	virtual ~device_lisabus_card_interface();

	void set_lisabus(lisabus_device *lisabus, const char *slottag) { m_lisabus = lisabus; m_lisabus_slottag = slottag; }

protected:

	// Subclass API

	virtual u16 card_r(offs_t off, u16 mask = ~0) = 0;
	virtual void card_w(offs_t off, u16 data, u16 mask = ~0) = 0;

	device_lisabus_card_interface(const machine_config &mconfig, device_t &device);

	virtual void interface_validity_check(validity_checker &valid) const override;
	virtual void interface_pre_start() override;

	void int_w(int level) { assert(m_lisabus); m_lisabus->slot_int_w(m_slot, level); }
	void iack_w(int level) { assert(m_lisabus); m_lisabus->slot_iack_w(m_slot, level); }

	int slotno() const { assert(m_lisabus); return m_slot; }
	lisabus_device &lisabus() { assert(m_lisabus); return *m_lisabus; }

private:
	optional_device<lisabus_device> m_lisabus_finder;
	lisabus_device *m_lisabus;
	const char *m_lisabus_slottag;
	int m_slot;
};

#endif // MAME_BUS_LISABUS_LISABUS_H
