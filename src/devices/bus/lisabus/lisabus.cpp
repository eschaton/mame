// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

  Apple Lisa expansion slot bus

  Based on Apple II bus implementation by R. Belmont.

    LisaBus Slot n

                 ____________
        DGND  - | 72      71 |- DGND
        DGND  - | 70      69 |- DGND
        DGND  - | 68      67 |- DGND
        DGND  - | 66      65 |- DGND
        DGND  - | 64      63 |- DGND
        DGND  - | 62      61 |- DGND
         +5V  - | 60      59 |- +5V
         +5V  - | 58      57 |- +5V
         +5V  - | 56      55 |- +5V
        DGND  - | 54      53 |- DGND
    +5VSTDBY  - | 52      51 |- SPKRIN
        /UDS  - | 50      49 |- /LDS
        READ  - | 48      47 |- /AS
      /DTACK  - | 46      45 |- +12V
        /VPA  - | 44      43 |- /VMA
         A12  - | 42      41 |- A11
         A10  - | 40      39 |- A9
          A8  - | 38      37 |- A7
          A6  - | 36      35 |- A5
          A4  - | 34      33 |- A3
          A2  - | 32      31 |- A1
         BD0  - | 30      29 |- BD1
         BD2  - | 28      27 |- BD3
         BD4  - | 26      25 |- BD5
         BD6  - | 24      23 |- BD7
         BD8  - | 22      21 |- BD9
        BD10  - | 20      19 |- BD11
        BD12  - | 18      17 |- BD13
        BD14  - | 16      15 |- BD15
     /BGOn-1  - | 14      13 |- /BGOn
         /BR  - | 12      11 |- E
      /BGACK  - | 10      9  |- /LDMA
       CPUCK  - |  8      7  |- /RESET
       /IAKn  - |  6      5  |- /INTn
        /SHn  - |  4      3  |- /SLn
        +12V  - |  2      1  |- -5V
                |____________|

    Signal      Dir.    Name
    ----------- ------- ------------------------------------------
    A1..A12     L-->S   Address Bus
    BD0..BD15   L<->S   Buffered Data Bus
    /UDS, /LDS  L-->S   Upper & Lower Data Strobe
    READ        L-->S   Read/Write
    /AS         L-->S   Address Strobe
    /DTACK      L<--S   Data Transfer Acknowledge
    /VPA        L<--S   Valid Peripheral Address
    /VMA        L<--S   Valid Memory Address
    /BGOn-1     L-->S   Bus Grant for slot n-1 (slot 1 is NC)
    /BGOn       L-->S   Bus Grant for slot n (slot 3 is NC)
    /BR         L<--S   Bus Request
    /BGACK      L<--S   Bus Grant Acknowledge
    /INTn       L<--S   Interrupt Request for slot n
    /IACKn      L-->S   Interrupt Acknowledge for slot n
    /SLn, /SHn  L-->S   Slot n Low/High access
    CPUCK       L-->S   CPU clock
    E           L-->S   1/8 CPU clock (6800 peripheral compat.)

***************************************************************************/

#include "emu.h"
#include "lisabus.h"


#include <iostream>

#define LOG_ACCESS          (1 << 1U)
#define LOGACCESS(...)      LOGMASKED(LOG_ACCESS, __VA_ARGS__)
// #define VERBOSE              (0)
#define VERBOSE             (LOG_GENERAL|LOG_ACCESS)
#define LOG_OUTPUT_STREAM   std::cout

#include "logmacro.h"


template class device_finder<device_lisabus_card_interface, false>;
template class device_finder<device_lisabus_card_interface, true>;

lisabus_slot_device::lisabus_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, LISABUS_SLOT, tag, owner, clock)
	, device_single_card_slot_interface<device_lisabus_card_interface>(mconfig, *this)
	, m_lisabus(*this, "lisabus")
{
}

void lisabus_slot_device::device_resolve_objects()
{
	device_lisabus_card_interface * const lisabus_card = get_card_device();
	if (lisabus_card)
		lisabus_card->set_lisabus(m_lisabus, tag());
}

void lisabus_slot_device::device_start()
{
}

void lisabus_slot_device::device_reset()
{
}

DEFINE_DEVICE_TYPE(LISABUS_SLOT, lisabus_slot_device, "lisabus_slot", "Apple Lisa Slot")


lisabus_device::lisabus_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: lisabus_device(mconfig, LISABUS, tag, owner, clock)
{
}

void lisabus_device::add_lisabus_card(int slot, device_lisabus_card_interface *card)
{
	m_device_list[slot] = card;
}

device_lisabus_card_interface *lisabus_device::get_lisabus_card(int slot)
{
	if ((slot >= 0) && (slot <= 2)) {
		return m_device_list[slot];
	} else {
		return nullptr;
	}

	return nullptr;

}

u16 lisabus_device::slot_r(int slot, offs_t offset, u16 mask)
{
	LOGACCESS("LISABUS: slot_r(%d, 0x%04x) & 0x%04x" "\n", slot, offset, mask);
	assert((slot >= 0) && (slot <= 2));
	u16 data;
	if (m_device_list[slot]) {
		data = m_device_list[slot]->card_r(offset, mask);
	} else {
		slot_berr_w(0);
		slot_berr_w(1);
		data = 0xff;
	}
	return data;
}

void lisabus_device::slot_w(int slot, offs_t offset, u16 data, u16 mask)
{
	LOGACCESS("LISABUS: slot_w(%d, 0x%04x, 0x%04x & 0x%04x" "\n", slot, offset, data, mask);
	assert((slot >= 0) && (slot <= 2));
	if (m_device_list[slot]) {
		m_device_list[slot]->card_w(offset, data, mask);
	} else {
		slot_berr_w(0);
		slot_berr_w(1);
	}
}

u16 lisabus_device::bus_r(offs_t offset, u16 mask)
{
	LOGACCESS("LISABUS: bus_r(0x%04x) & 0x%04x" "\n", offset, mask);
	u16 data;
	switch (offset & 0xc000) {
		case 0x0000: data = slot_r(0, offset, mask); break;
		case 0x4000: data = slot_r(1, offset, mask); break;
		case 0x8000: data = slot_r(2, offset, mask); break;
		default:
			slot_berr_w(0);
			slot_berr_w(1);
			data = 0xffff;
			break;
	}

	return data;
}

void lisabus_device::bus_w(offs_t offset, u16 data, u16 mask)
{
	LOGACCESS("LISABUS: bus_w(0x%04x, 0x%04x & 0x%04x" "\n", offset, data, mask);
	switch (offset & 0xc000) {
		case 0x0000: slot_w(0, offset, data, mask); break;
		case 0x4000: slot_w(1, offset, data, mask); break;
		case 0x8000: slot_w(2, offset, data, mask); break;
		default:
			slot_berr_w(0);
			slot_berr_w(1);
			break;
	}
}

void lisabus_device::device_start()
{
	std::fill(std::begin(m_device_list), std::end(m_device_list), nullptr);
}

void lisabus_device::device_reset()
{
}

DEFINE_DEVICE_TYPE(LISABUS, lisabus_device, "lisabus", "Apple Lisa Expansion Bus")


device_lisabus_card_interface::device_lisabus_card_interface(const machine_config &mconfig, device_t &device)
	: device_interface(device, "lisabus")
	, m_lisabus_finder(device, finder_base::DUMMY_TAG)
	, m_lisabus(nullptr)
	, m_lisabus_slottag(nullptr)
	, m_slot(-1)
{
}

device_lisabus_card_interface::~device_lisabus_card_interface()
{
}

void device_lisabus_card_interface::interface_validity_check(validity_checker &valid) const
{
	if (m_lisabus_finder && m_lisabus && (m_lisabus != m_lisabus_finder))
		osd_printf_error("Contradictory buses configured (%s and %s)\n", m_lisabus_finder->tag(), m_lisabus->tag());
}

void device_lisabus_card_interface::interface_pre_start()
{
	if (!m_lisabus) {
		m_lisabus = m_lisabus_finder;
		if (!m_lisabus)
			fatalerror("Can't find Apple Lisa Expansion Bus device %s\n", m_lisabus_finder.finder_tag());
	}

	if (m_slot < 0) {
		if (!m_lisabus->started())
			throw device_missing_dependencies();

		// extract the slot number from the last digit of the slot tag
		size_t const tlen = strlen(m_lisabus_slottag);

		m_slot = (m_lisabus_slottag[tlen - 1] - '0');
		if (m_slot < 0 || m_slot > 2)
			fatalerror("Slot %x out of range for Apple Lisa Expansion Bus\n", m_slot);

		m_lisabus->add_lisabus_card(m_slot, this);
	}
}
