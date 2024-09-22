// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/***************************************************************************

  sunp4.c - Sun P4 Bus slot bus and card emulation

***************************************************************************/

#include "emu.h"

// Display boards
#include "sunp4_mg.h"

#include "sunp4.h"

void sunp4_cards(device_slot_interface &device)
{
	device.option_add("sunp4_mg3",    SUNP4_MG3);   /* Sun P4 Bus MG3 ECL monochrome display board */
	device.option_add("sunp4_mg4",    SUNP4_MG4);   /* Sun P4 Bus MG4 Analog/ECL monochrome display board */
}

DEFINE_DEVICE_TYPE(SUNP4_SLOT, sunp4_slot_device, "sunp4_slot", "Sun P4 Bus Slot")

sunp4_slot_device::sunp4_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: sunp4_slot_device(mconfig, SUNP4_SLOT, tag, owner, clock)
{
}

sunp4_slot_device::sunp4_slot_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, device_single_card_slot_interface<device_sunp4_card_interface>(mconfig, *this)
	, m_sunp4(*this, finder_base::DUMMY_TAG)
	, m_slot(-1)
{
}

void sunp4_slot_device::device_validity_check(validity_checker &valid) const
{
	if (m_slot < 0 || m_slot > 0)
		osd_printf_error("Slot %d out of range for Sun P4 Bus\n", m_slot);
}

void sunp4_slot_device::device_resolve_objects()
{
	device_sunp4_card_interface *const sunp4_card(get_card_device());
	if (sunp4_card)
		sunp4_card->set_sunp4(m_sunp4, m_slot);
}

void sunp4_slot_device::device_start()
{
}


DEFINE_DEVICE_TYPE(SUNP4, sunp4_device, "sunp4", "Sun P4 Bus")

device_memory_interface::space_config_vector sunp4_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(0, &m_space_config)
	};
}

sunp4_device::sunp4_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: sunp4_device(mconfig, SUNP4, tag, owner, clock)
{
}

sunp4_device::sunp4_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, device_memory_interface(mconfig, *this)
	, m_space_config("sunp4", ENDIANNESS_BIG, 32, 32, 0, address_map_constructor())
	, m_maincpu(*this, finder_base::DUMMY_TAG)
	, m_type1space(*this, finder_base::DUMMY_TAG, -1)
	, m_irq_cb(*this)
	, m_buserr(*this)
{
}

void sunp4_device::device_start()
{
	std::fill(std::begin(m_device_list), std::end(m_device_list), nullptr);

	m_space = &space(0);
	m_space->install_readwrite_handler(0x00000000, 0x01ffffff, read32smo_delegate(*this, FUNC(sunp4_device::slot_timeout_r<0>)), write32smo_delegate(*this, FUNC(sunp4_device::slot_timeout_w<0>)));
	m_space->install_readwrite_handler(0x02000000, 0x03ffffff, read32smo_delegate(*this, FUNC(sunp4_device::slot_timeout_r<1>)), write32smo_delegate(*this, FUNC(sunp4_device::slot_timeout_w<1>)));
	m_space->install_readwrite_handler(0x04000000, 0x05ffffff, read32smo_delegate(*this, FUNC(sunp4_device::slot_timeout_r<2>)), write32smo_delegate(*this, FUNC(sunp4_device::slot_timeout_w<2>)));
}

template <unsigned Slot> uint32_t sunp4_device::slot_timeout_r()
{
	m_maincpu->set_mae();
	m_buserr(0, 0x20);
	m_buserr(1, 0xffa00000 + (Slot << 21));
	return 0;
}

template <unsigned Slot> void sunp4_device::slot_timeout_w(uint32_t data)
{
	m_maincpu->set_mae();
	m_buserr(0, 0x8020);
	m_buserr(1, 0xffa00000 + (Slot << 21));
}

uint32_t sunp4_device::read(offs_t offset, uint32_t mem_mask)
{
	return m_space->read_dword(offset << 2, mem_mask);
}

void sunp4_device::write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	m_space->write_dword(offset << 2, data, mem_mask);
}

device_sunp4_card_interface *sunp4_device::get_sunp4_card(int slot)
{
	if (slot < 0)
	{
		return nullptr;
	}

	if (m_device_list[slot])
	{
		return m_device_list[slot];
	}

	return nullptr;
}

void sunp4_device::add_sunp4_card(int slot, device_sunp4_card_interface *card)
{
	m_device_list[slot] = card;
	card->install_device();
}

void sunp4_device::set_irq_line(int state, int line)
{
	m_irq_cb[line](state);
}



device_sunp4_card_interface::device_sunp4_card_interface(const machine_config &mconfig, device_t &device)
	: device_interface(device, "sunp4")
	, m_sunp4_finder(device, finder_base::DUMMY_TAG)
	, m_sunp4(nullptr)
	, m_slot(-1)
	, m_base(0)
{
}

device_sunp4_card_interface::~device_sunp4_card_interface()
{
}

void device_sunp4_card_interface::interface_validity_check(validity_checker &valid) const
{
	if (m_sunp4_finder && m_sunp4 && (m_sunp4 != m_sunp4_finder))
		osd_printf_error("Contradictory buses configured (%s and %s)\n", m_sunp4_finder->tag(), m_sunp4->tag());
}

void device_sunp4_card_interface::interface_pre_start()
{
	if (!m_sunp4)
	{
		m_sunp4 = m_sunp4_finder;
		if (!m_sunp4)
			fatalerror("Can't find Sun P4 Bus device %s\n", m_sunp4_finder.finder_tag());
	}

	if (!m_sunp4->started())
		throw device_missing_dependencies();
}

void device_sunp4_card_interface::interface_post_start()
{
	m_base = m_slot << 25;
	m_sunp4->add_sunp4_card(m_slot, this);
}

void device_sunp4_card_interface::set_sunp4(sunp4_device *sunp4, int slot)
{
	m_sunp4 = sunp4;
	m_slot = slot;
}
