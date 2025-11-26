// license:BSD-3-Clause
// copyright-holders:R. Belmont
/****************************************************************************

    decstation.cpp: MIPS-based DECstation family

    WANTED: boot ROM dumps for KN02CA/KN04CA (MAXine) systems.

    NOTE: after all the spew of failing tests, press 'q' at the MORE prompt and
    wait a few seconds for the PROM monitor to appear.
    Type 'ls' for a list of commands (this is a very UNIX-flavored PROM monitor).

    Machine types:
        Personal DECstation 5000/xx (MAXine/KN02CA for R3000, KN04CA? for R4000)
            20, 25, or 33 MHz R3000 or 100 MHz R4000
            40 MiB max RAM
            Serial: DEC "DZ" quad-UART for keyboard/mouse, SCC8530 for modem/printer
            SCSI: NCR53C94
            Ethernet: AMD7990 "LANCE" controller
            Audio/ISDN: AMD AM79C30
            Color 1024x768 8bpp video on-board
            2 TURBOchannel slots

        DECstation 5000/1xx: (3MIN/KN02BA, KN04BA? for R4000):
            20, 25, or 33 MHz R3000 or 100 MHz R4000
            128 MiB max RAM
            Serial: 2x SCC8530
            SCSI: NCR53C94
            Ethernet: AMD7990 "LANCE" controller
            No on-board video
            3 TURBOchannel slots

        DECstation 5000/200: (3MAX/KN02):
            25 MHz R3000
            480 MiB max RAM
            Serial: DEC "DZ" quad-UART
            SCSI: NCR53C94
            Ethernet: AMD7990 "LANCE" controllor

        DECstation 5000/240 (3MAX+/KN03AA), 5000/260 (3MAX+/KN05)
            40 MHz R3400, or 120 MHz R4400.
            480 MiB max RAM
            Serial: 2x SCC8530
            SCSI: NCR53C94
            Ethernet: AMD7990 "LANCE" controller

****************************************************************************/

#include "emu.h"

#include "decioga.h"
#include "lk201.h"

#include "cpu/mips/mips1.h"

#include "machine/am79c90.h"
#include "machine/mc146818.h"
#include "machine/ncr53c90.h"
#include "machine/nscsi_bus.h"
#include "machine/ram.h"
#include "machine/z80scc.h"

#include "bus/nscsi/cd.h"
#include "bus/nscsi/hd.h"
#include "bus/rs232/rs232.h"
#include "bus/tc/tc.h"

#include "screen.h"

namespace {

class kn02ba_state : public driver_device
{
public:
	kn02ba_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_cpu(*this, "cpu")
		, m_tc(*this, "tc")
		, m_tcslot(*this, "tc:%u", 0U)
		, m_lk201(*this, "lk201")
		, m_ioga(*this, "ioga")
		, m_rtc(*this, "rtc")
		, m_scc(*this, "scc%u", 0U)
		, m_asc(*this, "scsi:7:asc")
		, m_lance(*this, "am79c90")
	{
	}

	void m120(machine_config &config) { kn02ba(config, 20'000'000); }
	void m125(machine_config &config) { kn02ba(config, 25'000'000); }
	void m133(machine_config &config) { kn02ba(config, 33'300'000); }

protected:
	void kn02ba(machine_config &config, u32 clock);

	uint32_t cfb_r(offs_t offset);
	void cfb_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

private:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	required_device<mips1_device_base> m_cpu;
	required_device<tc_device> m_tc;
	required_device_array<tc_slot_device, 3> m_tcslot;
	optional_device<lk201_device> m_lk201;
	required_device<dec_ioga_device> m_ioga;
	required_device<mc146818_device> m_rtc;
	required_device_array<z80scc_device, 2> m_scc;
	optional_device<ncr53c94_device> m_asc;
	required_device<am79c90_device> m_lance;

	void map(address_map &map) ATTR_COLD;
};

/***************************************************************************
    MACHINE FUNCTIONS
***************************************************************************/

void kn02ba_state::machine_start()
{
}

void kn02ba_state::machine_reset()
{
	m_ioga->set_dma_space(&m_cpu->space(AS_PROGRAM));
}

/***************************************************************************
    ADDRESS MAPS
***************************************************************************/

void kn02ba_state::map(address_map &map)
{
	map(0x00000000, 0x07ffffff).ram();  // full 128 MB
	map(0x1c000000, 0x1c07ffff).m(m_ioga, FUNC(dec_ioga_device::map));
	map(0x1c0c0000, 0x1c0c0007).rw(m_lance, FUNC(am79c90_device::regs_r), FUNC(am79c90_device::regs_w)).umask32(0x0000ffff);
	map(0x1c100000, 0x1c100003).rw(m_scc[0], FUNC(z80scc_device::ca_r), FUNC(z80scc_device::ca_w)).umask32(0x0000ff00);
	map(0x1c100004, 0x1c100007).rw(m_scc[0], FUNC(z80scc_device::da_r), FUNC(z80scc_device::da_w)).umask32(0x0000ff00);
	map(0x1c100008, 0x1c10000b).rw(m_scc[0], FUNC(z80scc_device::cb_r), FUNC(z80scc_device::cb_w)).umask32(0x0000ff00);
	map(0x1c10000c, 0x1c10000f).rw(m_scc[0], FUNC(z80scc_device::db_r), FUNC(z80scc_device::db_w)).umask32(0x0000ff00);
	map(0x1c180000, 0x1c180003).rw(m_scc[1], FUNC(z80scc_device::ca_r), FUNC(z80scc_device::ca_w)).umask32(0x0000ff00);
	map(0x1c180004, 0x1c180007).rw(m_scc[1], FUNC(z80scc_device::da_r), FUNC(z80scc_device::da_w)).umask32(0x0000ff00);
	map(0x1c180008, 0x1c18000b).rw(m_scc[1], FUNC(z80scc_device::cb_r), FUNC(z80scc_device::cb_w)).umask32(0x0000ff00);
	map(0x1c18000c, 0x1c18000f).rw(m_scc[1], FUNC(z80scc_device::db_r), FUNC(z80scc_device::db_w)).umask32(0x0000ff00);
	map(0x1c200000, 0x1c2000ff).rw(m_rtc, FUNC(mc146818_device::read_direct), FUNC(mc146818_device::write_direct)).umask32(0x000000ff);
	map(0x1c300000, 0x1c30003f).m(m_asc, FUNC(ncr53c94_device::map)).umask32(0x000000ff);
	map(0x1fc00000, 0x1fc3ffff).rom().region("user1", 0);

// 	tc_slot_device &tc_slot0 = TC_SLOT(config, "tc" ":0", tc_cards, "pmagb_ba");
// 	tc_slot_device &tc_slot1 = TC_SLOT(config, "tc" ":1", tc_cards, nullptr);
// 	tc_slot_device &tc_slot2 = TC_SLOT(config, "tc" ":2", tc_cards, nullptr);
// 	map(0x10000000, 0x13ffffff).rw("tc:0", FUNC(tc_device::read), FUNC(tc_device::write));
// 	map(0x14000000, 0x17ffffff).m("tc:1");
// 	map(0x18000000, 0x1bffffff).m("tc:2");
// 	map(0x10000000, 0x13ffffff).rw(m_tcslot[0], FUNC(tc_slot_device::read), FUNC(tc_slot_device::write));

}

/***************************************************************************
    MACHINE DRIVERS
***************************************************************************/

static void dec_scsi_devices(device_slot_interface &device)
{
	device.option_add("cdrom", NSCSI_CDROM);
	device.option_add("harddisk", NSCSI_HARDDISK);
}

void kn02ba_state::kn02ba(machine_config &config, u32 clock)
{
	R3000A(config, m_cpu, clock, 65536, 131072);
	m_cpu->set_endianness(ENDIANNESS_LITTLE);
	m_cpu->set_fpu(mips1_device_base::MIPS_R3010Av4);
	m_cpu->in_brcond<0>().set_constant(1);
	m_cpu->set_addrmap(AS_PROGRAM, &kn02ba_state::map);

	// The DECstation 5000 Model 100 series has three TURBOchannel
	// slots, 0 through 2, plus a virtual slot 3 for system board
	// peripherals.
	//
	// These are mapped to the following addresses and interrupts,
	// according to "TURBOchannel System Parameters" (EK-TCAAB-SP-005):
	//
	// - 0: 0x10000000 CPU interrupt 0
	// - 1: 0x14000000 CPU interrupt 1
	// - 2: 0x18000000 CPU interrupt 2
	// - 3: 0x1c000000 no directly corresponding interrupt

	TC(config, m_tc, 12'500'000);
	m_tc->set_space(m_cpu, AS_PROGRAM);
	m_tcslot[0] = TC_SLOT(config, "tc" ":0", tc_cards, "pmagb_ba");
	m_tcslot[1] = TC_SLOT(config, "tc" ":1", tc_cards, nullptr);
	m_tcslot[2] = TC_SLOT(config, "tc" ":2", tc_cards, nullptr);
	m_tcslot[0]->int_cb().set_inputline(m_cpu, INPUT_LINE_IRQ0);
	m_tcslot[1]->int_cb().set_inputline(m_cpu, INPUT_LINE_IRQ1);
	m_tcslot[2]->int_cb().set_inputline(m_cpu, INPUT_LINE_IRQ2);

	AM79C90(config, m_lance, XTAL(12'500'000));
	m_lance->intr_out().set("ioga", FUNC(dec_ioga_device::lance_irq_w));
	m_lance->dma_in().set("ioga", FUNC(dec_ioga_device::lance_dma_r));
	m_lance->dma_out().set("ioga", FUNC(dec_ioga_device::lance_dma_w));

	DECSTATION_IOGA(config, m_ioga, XTAL(12'500'000));
	m_ioga->irq_out().set_inputline(m_cpu, INPUT_LINE_IRQ3);

	MC146818(config, m_rtc, XTAL(32'768));
	m_rtc->irq().set("ioga", FUNC(dec_ioga_device::rtc_irq_w));
	m_rtc->set_binary(true);

	SCC85C30(config, m_scc[0], XTAL(14'745'600)/2);
	m_scc[0]->out_int_callback().set("ioga", FUNC(dec_ioga_device::scc0_irq_w));
	m_scc[0]->out_txda_callback().set("rs232a", FUNC(rs232_port_device::write_txd));
	m_scc[0]->out_txdb_callback().set("rs232b", FUNC(rs232_port_device::write_txd));

	SCC85C30(config, m_scc[1], XTAL(14'745'600)/2);
	m_scc[1]->out_int_callback().set("ioga", FUNC(dec_ioga_device::scc1_irq_w));
	m_scc[1]->out_txdb_callback().set(m_lk201, FUNC(lk201_device::rx_w));

	LK201(config, m_lk201, 0);
	m_lk201->tx_handler().set(m_scc[1], FUNC(z80scc_device::rxb_w));

	rs232_port_device &rs232a(RS232_PORT(config, "rs232a", default_rs232_devices, nullptr));
	rs232a.rxd_handler().set(m_scc[0], FUNC(z80scc_device::rxa_w));
	rs232a.dcd_handler().set(m_scc[0], FUNC(z80scc_device::dcda_w));
	rs232a.cts_handler().set(m_scc[0], FUNC(z80scc_device::ctsa_w));

	rs232_port_device &rs232b(RS232_PORT(config, "rs232b", default_rs232_devices, nullptr));
	rs232b.rxd_handler().set(m_scc[0], FUNC(z80scc_device::rxb_w));
	rs232b.dcd_handler().set(m_scc[0], FUNC(z80scc_device::dcdb_w));
	rs232b.cts_handler().set(m_scc[0], FUNC(z80scc_device::ctsb_w));

	NSCSI_BUS(config, "scsi");
	NSCSI_CONNECTOR(config, "scsi:0", dec_scsi_devices, "harddisk");
	NSCSI_CONNECTOR(config, "scsi:1", dec_scsi_devices, "cdrom");
	NSCSI_CONNECTOR(config, "scsi:2", dec_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:3", dec_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:4", dec_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:5", dec_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:6", dec_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:7").option_set("asc", NCR53C94).clock(10_MHz_XTAL).machine_config(
		[this](device_t *device)
		{
			ncr53c94_device &asc = downcast<ncr53c94_device &>(*device);

			asc.irq_handler_cb().set_inputline(m_cpu, INPUT_LINE_IRQ0);
		});
}

static INPUT_PORTS_START(kn02ba)
	PORT_START("UNUSED") // unused IN0
	PORT_BIT(0xffff, IP_ACTIVE_HIGH, IPT_UNUSED)
INPUT_PORTS_END

/***************************************************************************

  ROM definition(s)

***************************************************************************/

ROM_START( ds5k133 )
	ROM_REGION32_LE( 0x40000, "user1", 0 )
	// 5.7j                                                                                                                                                                                                                                 sx
	ROM_LOAD( "ds5000-133_005eb.bin", 0x000000, 0x040000, CRC(76a91d29) SHA1(140fcdb4fd2327daf764a35006d05fabfbee8da6) )
ROM_END

} // anonymous namespace

//    YEAR  NAME     PARENT  COMPAT  MACHINE  INPUT   CLASS         INIT  COMPANY                          FULLNAME               FLAGS
//COMP( 1992, ds5k20,  0,      0,      m20,     kn02ca, kn02ca_state, init, "Digital Equipment Corporation", "DECstation 5000/20",  MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
//COMP( 1992, ds5k120, 0,      0,      m120,    kn02ba, kn02ba_state, init, "Digital Equipment Corporation", "DECstation 5000/120", MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
COMP( 1992, ds5k133, 0,      0,      m133,    kn02ba, kn02ba_state, empty_init, "Digital Equipment Corporation", "DECstation 5000/133", MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
