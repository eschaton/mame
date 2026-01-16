// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

  Apple Lisa Dual Parallel Port Card

***************************************************************************/

#include "lisadualpp.h"

#include "machine/6522via.h"
#include "machine/input_merger.h"

#include <iostream>

#define LOG_ACCESS          (1 << 1U)
#define LOGACCESS(...)      LOGMASKED(LOG_ACCESS, __VA_ARGS__)
// #define VERBOSE              (0)
#define VERBOSE             (LOG_GENERAL|LOG_ACCESS)
#define LOG_OUTPUT_STREAM   std::cout

#include "logmacro.h"


class lisadualpp_card_device : public device_t, public device_lisabus_card_interface
{
public:
	lisadualpp_card_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	lisadualpp_card_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
		: device_t(mconfig, type, tag, owner, clock)
		, device_lisabus_card_interface(mconfig, *this)
		, m_irqm(*this, "irqm")
		, m_via0(*this, "via0")
		, m_via1(*this, "via1")
	{}

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u16 card_r(offs_t off, u16 mask = ~0) override;
	virtual void card_w(offs_t off, u16 data, u16 mask = ~0) override;

private:
	u16 rom_r(offs_t off);

	required_device<input_merger_device> m_irqm;
	required_device<via6522_device> m_via0;
	required_device<via6522_device> m_via1;
};


lisadualpp_card_device::lisadualpp_card_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: lisadualpp_card_device(mconfig, LISADUALPP, tag, owner, clock)
{
}

void lisadualpp_card_device::device_add_mconfig(machine_config &config)
{
	printf("lisadualpp_card_device::device_add_mconfig" "\n");
	INPUT_MERGER_ANY_HIGH(config, m_irqm);
	m_irqm->output_handler().set(FUNC(lisadualpp_card_device::int_w));

	MOS6522(config, m_via0, 20.37504_MHz_XTAL / 16); // high-speed like Lisa 2/10
	m_via0->irq_handler().set(m_irqm, FUNC(input_merger_device::in_w<0>));

	MOS6522(config, m_via1, 20.37504_MHz_XTAL / 16); // high-speed like Lisa 2/10
	m_via1->irq_handler().set(m_irqm, FUNC(input_merger_device::in_w<1>));
}

void lisadualpp_card_device::device_start()
{
	printf("lisadualpp_card_device::device_start" "\n");
}

void lisadualpp_card_device::device_reset()
{
	printf("lisadualpp_card_device::device_reset" "\n");
}

u16 lisadualpp_card_device::card_r(offs_t off, u16 mask)
{
	LOGACCESS("%s: card_r(0x%04x) & 0x%04x (%s)" "\n", name(), off, mask, machine().describe_context());

	u16 data;

	switch (off & 0xff01) {
		case 0x0001:    data = rom_r(off);          break;
		case 0x2000:    data = m_via0->read(off);   break;
		case 0x2800:    data = m_via1->read(off);   break;
		default:
			data = 0xffff;
			lisabus().slot_berr_w(0);
			lisabus().slot_berr_w(1);
			break;
	}

	return data & mask;
}

void lisadualpp_card_device::card_w(offs_t off, u16 data, u16 mask)
{
	LOGACCESS("%s: card_w(0x%04x, 0x%04x & 0x%04x) (%s)" "\n", name(), off, data, mask, machine().describe_context());

	switch (off & 0xff00) {
		case 0x2000:    m_via0->write(off, data);   break;
		case 0x2800:    m_via1->write(off, data);   break;
		default:
			lisabus().slot_berr_w(0);
			lisabus().slot_berr_w(1);
			break;
	}
}

u16 lisadualpp_card_device::rom_r(offs_t off)
{
	switch (off) {
		case 0x0001:    return 0x0080;
		case 0x0003:    return 0x0003;
		default:        return 0xffff;
	}
}


DEFINE_DEVICE_TYPE_PRIVATE(LISADUALPP, device_lisabus_card_interface, lisadualpp_card_device, "lisadualpp", "Apple Lisa Dual Parallel Port Card")
