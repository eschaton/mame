// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/**********************************************************************

    DEC TURBOchannel emulation (skeleton)

**********************************************************************/

#include "emu.h"

#include "tc.h"

// Video Cards
#include "pmagb_ba.h"


void tc_cards(device_slot_interface &device)
{
	device.option_add("pmagb_ba", PMAGB_BA);
}


DEFINE_DEVICE_TYPE(TC, tc_device, "tc", "DEC TURBOchannel bus")
DEFINE_DEVICE_TYPE(TC_SLOT, tc_slot_device, "tc_slot", "DEC TURBOchannel slot")


device_tc_card_interface::device_tc_card_interface(const machine_config &mconfig, device_t &device)
	: device_interface(device, "tc")
	, m_bus(nullptr)
{
}


tc_slot_device::tc_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, TC_SLOT, tag, owner, clock)
	, device_slot_interface(mconfig, *this)
	, m_card(nullptr)
	, m_bus(*this, DEVICE_SELF_OWNER)
{
}


void tc_slot_device::device_start()
{
	device_tc_card_interface *dev = dynamic_cast<device_tc_card_interface *>(get_card_device());
	if (dev) {
		m_bus->add_card(*dev);
	}
}

void tc_slot_device::device_reset()
{
}


tc_device::tc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, TC, tag, owner, clock)
	, device_memory_interface(mconfig, *this)
	, m_program_config("a32", ENDIANNESS_LITTLE, 32, 32, 0, address_map_constructor())
	, m_space(*this, finder_base::DUMMY_TAG, -1)
	, m_out_int_cb(*this)
{
}

tc_device::~tc_device()
{
}

device_memory_interface::space_config_vector tc_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(AS_PROGRAM, &m_program_config)
	};
}

void tc_device::device_start()
{
	m_view = nullptr;
}

void tc_device::device_reset()
{
}

void tc_device::add_card(device_tc_card_interface &card)
{
	card.m_bus = this;
	m_device_list.emplace_back(card);
	card.install_device();
}

uint16_t tc_device::read(offs_t offset, uint16_t mem_mask)
{
	return m_space->read_word(offset, mem_mask);
}

void tc_device::write(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	m_space->write_word(offset, data, mem_mask);
}
