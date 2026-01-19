// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/***************************************************************************

  Apple Lisa Dual Parallel Port Card

    - ROM is 341-0193-A and is bootable

***************************************************************************/

#include "lisadualpp.h"

#include "bus/applepp/applepp.h"

#include "machine/6522via.h"
#include "machine/74244.h"
#include "machine/74245.h"
#include "machine/input_merger.h"

#include <iostream>

#define LOG_ACCESS          (1 << 1U)
#define LOG_VIA0			(1 << 2U)
#define LOG_VIA1			(1 << 3U)
#define LOGACCESS(...)      LOGMASKED(LOG_ACCESS, "ACCESS: " __VA_ARGS__)
#define LOGVIA0(...)		LOGMASKED(LOG_VIA0, "VIA0: " __VA_ARGS__)
#define LOGVIA1(...)		LOGMASKED(LOG_VIA1, "VIA1: " __VA_ARGS__)
// #define VERBOSE             (0)
#define VERBOSE             (LOG_GENERAL|LOG_ACCESS|LOG_VIA0|LOG_VIA1)
#define LOG_OUTPUT_STREAM   std::cout

#include "logmacro.h"


ROM_START(lisadualpp)
	ROM_REGION(0x0800, "rom", 0)
	ROM_LOAD( "341-0193-a.bin", 0x0000, 0x0800, CRC(48c96d3e) SHA1(9e7f7dc042c9082662ef31c1900920e40a0894dd) )
ROM_END


class lisadualpp_card_device : public device_t, public device_lisabus_card_interface
{
public:
	lisadualpp_card_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	lisadualpp_card_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
		: device_t(mconfig, type, tag, owner, clock)
		, device_lisabus_card_interface(mconfig, *this)
		, m_rom(*this, "rom")
		, m_irqm(*this, "irqm")
		, m_via0(*this, "via0")
		, m_via1(*this, "via1")
		, m_ppctrlbuf0(*this, "ppctrlbuf0")
		, m_ppctrlbuf1(*this, "ppctrlbuf1")
		, m_ppdatabuf0(*this, "ppdatabuf0")
		, m_ppdatabuf1(*this, "ppdatabuf1")
		, m_lowerpp(*this, "lowerpp")
		, m_upperpp(*this, "upperpp")
	{}

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u16 card_r(offs_t off, u16 mask = ~0) override;
	virtual void card_w(offs_t off, u16 data, u16 mask = ~0) override;

	virtual void iack_w(int level) override;

private:
	u16 rom_r(offs_t off);

	u16 via_r(int n, offs_t off);
	void via_w(int n, offs_t off, u16 data);

	required_region_ptr<u8> m_rom;
	required_device<input_merger_device> m_irqm;
	required_device<mos6522_device> m_via0;
	required_device<mos6522_device> m_via1;
	required_device<ttl74244_device> m_ppctrlbuf0;
	required_device<ttl74244_device> m_ppctrlbuf1;
	required_device<ttl74245_device> m_ppdatabuf0;
	required_device<ttl74245_device> m_ppdatabuf1;
	required_device<applepp_connector> m_lowerpp;
	required_device<applepp_connector> m_upperpp;
};


lisadualpp_card_device::lisadualpp_card_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: lisadualpp_card_device(mconfig, LISADUALPP, tag, owner, clock)
{
}

void lisadualpp_card_device::device_add_mconfig(machine_config &config)
{
	INPUT_MERGER_ANY_HIGH(config, m_irqm);
	m_irqm->output_handler().set(FUNC(lisadualpp_card_device::int_w));

	// Lower Port

	MOS6522(config, m_via0, 20.37504_MHz_XTAL / 16); // high-speed like Lisa 2/10
	m_via0->irq_handler().set(m_irqm, FUNC(input_merger_device::in_w<0>));

	APPLEPP_CONNECTOR(config, m_lowerpp, applepp_intf, nullptr);
	TTL74244(config, m_ppctrlbuf0, 0);
	TTL74245(config, m_ppdatabuf0, 0);

	m_lowerpp->write_pd_from_device().set(m_ppdatabuf0, FUNC(ttl74245_device::a_w));
	m_ppdatabuf0->qa_cb().set(m_lowerpp, FUNC(applepp_connector::pd_set_from_host));
	m_via0->writepa_handler().set(m_ppdatabuf0, FUNC(ttl74245_device::b_w));
	m_ppdatabuf0->qb_cb().set(m_via0, FUNC(mos6522_device::write_pa));

	m_via0->ca2_handler().set(m_ppctrlbuf0, FUNC(ttl74244_device::a_w<0>)); // 1A1
	m_ppctrlbuf0->qa_cb<0>().set(m_lowerpp, FUNC(applepp_connector::pstrb_w)); // 1Y1
	m_ppctrlbuf0->qa_cb<1>().set(m_lowerpp, FUNC(applepp_connector::prw_w)); // 1Y2
	m_ppctrlbuf0->qa_cb<2>().set(m_lowerpp, FUNC(applepp_connector::pcmd_w)); // 1Y3
	m_lowerpp->write_pparity().set(m_ppctrlbuf0, FUNC(ttl74244_device::a_w<3>)); // 1A4
	m_ppctrlbuf0->qa_cb<3>().set(m_via0, FUNC(mos6522_device::write_pb6)); // 1Y4
	// Leaving 2A1 & 2Y1 unconnected because it treats /RESET as input,
	// and is in a wire-or with PB5 to reset the parity flipflop.
	m_lowerpp->write_pchk().set(m_ppctrlbuf0, FUNC(ttl74244_device::b_w<1>)); // 2A2
	m_ppctrlbuf0->qb_cb<1>().set(m_via0, FUNC(mos6522_device::write_pb0)); // 2Y2
	m_lowerpp->write_pbsy().set(m_ppctrlbuf0, FUNC(ttl74244_device::b_w<2>)); // 2A3
	m_ppctrlbuf0->qb_cb<2>().set(m_via0, FUNC(mos6522_device::write_pb1)); // 2Y3

	m_via0->writepb_handler().set(  [this](u8 data) {
										m_ppctrlbuf0->a_w<1>(BIT(data, 3)); // 1A2
										m_ppctrlbuf0->a_w<2>(BIT(data, 4)); // 1A3
										m_ppctrlbuf0->ga_w(BIT(data, 2));
										m_ppdatabuf0->oe_w(BIT(data, 2));
										m_ppdatabuf0->dir_w(BIT(data, 3));
									});

	m_ppctrlbuf0->qb_cb<3>().set(m_via0, FUNC(mos6522_device::write_pb1)); // 2Y4
	m_ppctrlbuf0->qb_cb<3>().append(m_via0, FUNC(mos6522_device::write_ca1)); // +2Y4

	// Upper Port

	MOS6522(config, m_via1, 20.37504_MHz_XTAL / 16); // high-speed like Lisa 2/10
	m_via1->irq_handler().set(m_irqm, FUNC(input_merger_device::in_w<0>));

	APPLEPP_CONNECTOR(config, m_upperpp, applepp_intf, nullptr);
	TTL74244(config, m_ppctrlbuf1, 0);
	TTL74245(config, m_ppdatabuf1, 0);

	m_upperpp->write_pd_from_device().set(m_ppdatabuf1, FUNC(ttl74245_device::a_w));
	m_ppdatabuf1->qa_cb().set(m_upperpp, FUNC(applepp_connector::pd_set_from_host));
	m_via1->writepa_handler().set(m_ppdatabuf1, FUNC(ttl74245_device::b_w));
	m_ppdatabuf1->qb_cb().set(m_via1, FUNC(mos6522_device::write_pa));

	m_via1->ca2_handler().set(m_ppctrlbuf1, FUNC(ttl74244_device::a_w<0>)); // 1A1
	m_ppctrlbuf1->qa_cb<0>().set(m_upperpp, FUNC(applepp_connector::pstrb_w)); // 1Y1
	m_ppctrlbuf1->qa_cb<1>().set(m_upperpp, FUNC(applepp_connector::prw_w)); // 1Y2
	m_ppctrlbuf1->qa_cb<2>().set(m_upperpp, FUNC(applepp_connector::pcmd_w)); // 1Y3
	m_upperpp->write_pparity().set(m_ppctrlbuf1, FUNC(ttl74244_device::a_w<3>)); // 1A4
	m_ppctrlbuf1->qa_cb<3>().set(m_via1, FUNC(mos6522_device::write_pb6)); // 1Y4
	// Leaving 2A1 & 2Y1 unconnected because it treats /RESET as input,
	// and is in a wire-or with PB5 to reset the parity flipflop.
	m_upperpp->write_pchk().set(m_ppctrlbuf1, FUNC(ttl74244_device::b_w<1>)); // 2A2
	m_ppctrlbuf1->qb_cb<1>().set(m_via1, FUNC(mos6522_device::write_pb0)); // 2Y2
	m_upperpp->write_pbsy().set(m_ppctrlbuf1, FUNC(ttl74244_device::b_w<2>)); // 2A3
	m_ppctrlbuf1->qb_cb<2>().set(m_via1, FUNC(mos6522_device::write_pb1)); // 2Y3

	m_via1->writepb_handler().set(  [this](u8 data) {
										m_ppctrlbuf1->a_w<1>(BIT(data, 3)); // 1A2
										m_ppctrlbuf1->a_w<2>(BIT(data, 4)); // 1A3
										m_ppctrlbuf1->ga_w(BIT(data, 2));
										m_ppdatabuf1->oe_w(BIT(data, 2));
										m_ppdatabuf1->dir_w(BIT(data, 3));
									});

	m_ppctrlbuf1->qb_cb<3>().set(m_via1, FUNC(mos6522_device::write_pb1)); // 2Y4
	m_ppctrlbuf1->qb_cb<3>().append(m_via1, FUNC(mos6522_device::write_ca1)); // +2Y4
}

const tiny_rom_entry *lisadualpp_card_device::device_rom_region() const
{
	return ROM_NAME(lisadualpp);
}

void lisadualpp_card_device::device_start()
{
	// port B/2 on the control buffers is always passthrough
	m_ppctrlbuf0->gb_w(0);
	m_ppctrlbuf1->gb_w(0);
}

void lisadualpp_card_device::device_reset()
{
}

u16 lisadualpp_card_device::card_r(offs_t off, u16 mask)
{
	u16 data;

	switch (off & 0xff00) {
		case 0x0000:    data = rom_r(off - 0x0000);     break;
		case 0x2000:    data = via_r(0, off - 0x2000);  break;
		case 0x2800:    data = via_r(1, off - 0x2800);  break;
		default:
			data = 0xffff;
			lisabus().slot_berr_w(0);
			lisabus().slot_berr_w(1);
			break;
	}

	LOGACCESS("%s: card_r(0x%04x) -> 0x%04x & 0x%04x (%s)" "\n", name(), off, data, mask, machine().describe_context());

	return data & mask;
}

void lisadualpp_card_device::card_w(offs_t off, u16 data, u16 mask)
{
	LOGACCESS("%s: card_w(0x%04x, 0x%04x & 0x%04x) (%s)" "\n", name(), off, data, mask, machine().describe_context());

	switch (off & 0xff00) {
		case 0x2000:    via_w(0, off - 0x2000, data);   break;
		case 0x2800:    via_w(1, off - 0x2800, data);   break;
		default:
			lisabus().slot_berr_w(0);
			lisabus().slot_berr_w(1);
			break;
	}
}

void lisadualpp_card_device::iack_w(int level)
{
	vpa_w(level);
}

u16 lisadualpp_card_device::rom_r(offs_t off)
{
	// the ROM is only 8 bits wide and read from odd addresses into a
	// buffer to use during boot

	u16 data = data = m_rom[off >> 1];

	LOGACCESS("%s: rom_r(0x%04x) -> 0x%04x (%s)" "\n", name(), off, data, machine().describe_context());

	return data;
}

u16 lisadualpp_card_device::via_r(int n, offs_t off)
{
	LOGACCESS("%s: via_r(0x%04x) (%s)" "\n", name(), off, machine().describe_context());

	mos6522_device *via = (n == 0) ? m_via0 : m_via1;
	offs_t real_off = off >> 3;
	u16 data = via->read(real_off);
	return data;
}

void lisadualpp_card_device::via_w(int n, offs_t off, u16 data)
{
	LOGACCESS("%s: via_w(0x%04x, 0x%04x) (%s)" "\n", name(), off, data, machine().describe_context());

	mos6522_device *via = (n == 0) ? m_via0 : m_via1;
	offs_t real_off = off >> 3;
	via->write(real_off, data & 0x00ff);
}

DEFINE_DEVICE_TYPE_PRIVATE(LISADUALPP, device_lisabus_card_interface, lisadualpp_card_device, "lisadualpp", "Apple Lisa Dual Parallel Port Card")
