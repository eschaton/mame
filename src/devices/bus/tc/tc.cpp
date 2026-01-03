// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/**********************************************************************

    DEC TURBOchannel emulation

**********************************************************************/

#include "emu.h"

#include "tc.h"

// Video Cards
#include "pmagb_ba.h"


#define LOG_CARD        (1U << 1)
#define LOG_SLOT        (1U << 2)

#define VERBOSE         (LOG_GENERAL|LOG_CARD|LOG_SLOT)
#define LOG_OUTPUT_FUNC printf

#include "logmacro.h"


void tc_cards(device_slot_interface &device)
{
	device.option_add("pmagb_ba", PMAGB_BA);
}


DEFINE_DEVICE_TYPE(TC, tc_device, "tc", "DEC TURBOchannel bus")
DEFINE_DEVICE_TYPE(TC_SLOT, tc_slot_device, "tc_slot", "DEC TURBOchannel slot")


device_tc_card_interface::device_tc_card_interface(const machine_config &mconfig, device_t &device)
	: device_interface(device, "tc")
	, m_bus(nullptr)
	, m_slot(nullptr)
	, m_out_int_cb(*this)
{
}


tc_slot_device::tc_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, TC_SLOT, tag, owner, clock)
	, device_slot_interface(mconfig, *this)
	, m_card(nullptr)
	, m_addrstart(0)
	, m_addrend(0)
	, m_bus(*this, DEVICE_SELF_OWNER)
	, m_out_int_cb(*this)
{
}

void tc_slot_device::device_start()
{
	LOGMASKED(LOG_SLOT, "TC_SLOT: Start\n");

	device_tc_card_interface *dev = dynamic_cast<device_tc_card_interface *>(get_card_device());

	if (dev) {
		m_bus->add_card(*dev, *this);
	}
}

void tc_slot_device::device_reset()
{
	LOGMASKED(LOG_SLOT, "TC_SLOT: Reset\n");
}


tc_device::tc_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, TC, tag, owner, clock)
	, device_memory_interface(mconfig, *this)
	, m_program_config("program", ENDIANNESS_LITTLE, 32, 32)
	, m_space(*this, finder_base::DUMMY_TAG, -1)
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
	LOG("TC: Started\n");
}

void tc_device::device_reset()
{
	LOG("TC: Reset\n");
}

void tc_device::add_card(device_tc_card_interface &card, tc_slot_device &slot)
{
	LOG("TC: Added card\n");
	card.m_bus = this;
	m_device_list.emplace_back(card);
	card.install_device(&slot);
}
