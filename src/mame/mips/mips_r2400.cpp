// license:BSD-3-Clause
// copyright-holders:Chris Hanson

/*
 * An emulation of the MIPS Computer Systems R2400 board, used by their M/120,
 * M/180, and RC3240 systems.
 */

#include "emu.h"

#include "cpu/mips/mips1.h"

#include "machine/am79c90.h"
#include "machine/am9516.h"
#include "machine/input_merger.h"
#include "machine/mb87030.h"
#include "machine/mc68681.h"
#include "machine/nscsi_bus.h"
#include "machine/pit8253.h"
#include "machine/ram.h"
#include "machine/timekpr.h"

#include "bus/nscsi/cd.h"
#include "bus/nscsi/devices.h"
#include "bus/nscsi/hd.h"
#include "bus/nscsi/tape.h"
#include "bus/rs232/rs232.h"


#define VERBOSE 0
#include "logmacro.h"


namespace {

/*
    R2400 Memory Map

        Taken from "M/120 RISComputer System Technical Reference".

        Overall Memory Map

        Addr Start  Addr End    Assignment          Size    Notes
        ----------  ----------  ------------------- ------  ---------
        0x00000000  0x007fffff  R2450 Main Memory     8 MB  Slot 1
        0x00800000  0x00ffffff  R2450 Main Memory     8 MB  Slot 2
        0x01000000  0x017fffff  R2450 Main Memory     8 MB  Slot 3
        0x01800000  0x01ffffff  R2450 Main Memory     8 MB  Slot 4
        0x02000000  0x027fffff  R2450 Main Memory     8 MB  Slot 5
        0x02800000  0x02ffffff  R2450 Main Memory     8 MB  Slot 6
        0x03000000  0x07ffffff  reserved             80 MB
        0x08000000  0x0fffffff  unused              128 MB
        0x10000000  0x17ffffff  PC/AT I/O & memory  128 MB
        0x18000000  0x1cffffff  Local I/O            96 MB  See below
        0x1d000000  0x1dffffff  Ethernet PROM        16 MB
        0x1e000000  0x1effffff  ID PROM              16 MB
        0x1f000000  0x1fffffff  Boot PROM            16 MB


        PC/AT Bus Memory Map

        Addr Start  Addr End    Cycle   Type    Swap
        ----------  ----------  -----   ------  -------
        0x10000000  0x10ffffff  CPU     Memory  Swap
        0x11000000  0x11ffffff  CPU     Memory  No Swap
        0x12000000  0x12ffffff  CPU     I/O     Swap
        0x13000000  0x13ffffff  CPU     I/O     No Swap
        0x14000000  0x14ffffff  DMA     Memory  Swap
        0x15000000  0x15ffffff  DMA     Memory  No Swap
        0x16000000  0x16ffffff  DMA     I/O     Swap
        0x17000000  0x17ffffff  DMA     I/O     No Swap


        Local I/O Memory Map

        Addr Start  Addr End    Assignment                      Width
        ----------  ----------  ------------------------------- -----
        0x18000002  0x18000003  System Configuration Register   16
        0x18010002  0x18010003  Interrupt Status Register       16
        0x18020002  0x18020003  Interrupt Mask Register         16
        0x18030000  0x18030003  Fault Address Register          32
        0x18040002  0x18040003  Fault ID Register               16
        0x18050003  0x18050003  Timer 0 Acknowledge              8
        0x18060003  0x18060003  Timer 1 Acknowledge              8
        0x18070002  0x18070003  AT Control Register             16
        0x18080003  0x18080003  LED Register                     8
        0x18090003  0x1809003f  DUART 0                          8
        0x180a0003  0x180a003f  DUART 1                          8
        0x180b0003  0x180b1fff  Clock/Calendar/NVRAM             8
        0x180c0003  0x180c00ff  Interval Timers                  8
        0x180d0003  0x180d00f3  SCSI Controller                  8
        0x180e0002  0x1800000a  DMA Controller                  16
        0x180f0002  0x180f0006  Ethernet Controller             16
        0x1b000002  0x1b000002  AT DAck Enable Register         16

        ID PROM Memory Map

        Address     Contents
        ----------  -------
        0x1e000000  Board Type (R2400=4)
        0x1e000007  Revision Level
                        0x10 = M/120-5, 16MHz
                        0x20 = M/120-3, 12.5MHz
        0x1e00000b  Serial number digit 0
        0x1e00000f  Serial number digit 1
        0x1e000013  Serial number digit 2
        0x1e000017  Serial number digit 3
        0x1e00001b  Serial number digit 4
        0x1e00001f  Checksum

        Interrupt map

        Int Purpose
        --- ---------------------
        IM5 Non-CPU Read Error
        IM4 Timer 1
        IM3 FPA
        IM2 Timer 0
        IM1 DUART
        IM0 Aggregated Interrupts

        Aggregated Interrupt Map

        Int Purpose
        --- ---------------------
        15  MBus
        14  Ethernet
        13  SCSI
        12  DMA
        11  Reserved
        10  PC/AT IRQ11
         9  PC/AT IRQ10
         8  PC/AT IRQ9
         7  PC/AT IRQ15
         6  PC/AT IRQ14
         5  PC/AT IRQ12
         4  PC/AT IRQ7
         3  PC/AT IRQ6
         2  PC/AT IRQ5
         1  PC/AT IRQ4
         0  PC/AT IOChCkB or IRQ3
 */
class mips_r2400_state : public driver_device
{
public:
	mips_r2400_state(machine_config const &mconfig, device_type type, char const *tag)
		: driver_device(mconfig, type, tag)
		, m_cpu(*this, "cpu")
		, m_ram(*this, "ram")
		, m_rom(*this, "r2400")
		, m_duart(*this, "duart%u", 0U)
		, m_duart_irqs(*this, "duart_irqs")
		, m_sio(*this, "sio%u", 0U)
		, m_scsibus(*this, "scsi")
		, m_scsi(*this, "scsi:7:mb87030")
		, m_net(*this, "net")
		, m_rtc(*this, "rtc")
		, m_pit(*this, "pit")
		, m_dma(*this, "dma")
		, m_model(model_unknown)
		, m_syscfg(0)
		, m_isr(0)
		, m_imr(0)
		, m_far(0)
		, m_fid(0)
		, m_led(0)
		, m_timer0_int(0)
		, m_timer1_int(0)
		, m_atc(0)
	{
	}

	// machine config
	void r2400(machine_config &config);

	void m120_5(machine_config &config);
	void m120_3(machine_config &config);
	void rc3240(machine_config &config);

	void r2400_init();

protected:
	// driver_device overrides
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

	// address maps
	void r2400_map(address_map &map) ATTR_COLD;

	// Bits in the R2400 System Configuration Register.
	enum syscfg_bit {
		// Read-only bits
		Key0        = 0,
		Pointer0    = 1,
		Pointer1    = 2,
		BootLockB   = 3,
		ColdStart   = 4,
		Rsvd0       = 5,
		Rsvd1       = 6,
		CoProcB     = 7,

		// Read-write bits
		ResetSCSI   = 8,
		SCSIHIN     = 9,
		SoftEOP     = 10,
		ResetPCATB  = 11,
		ATTCEn      = 12,
		SlowUDCEn   = 13,
		ForceBadPtr = 14,
		ParityEn    = 15,
	};

	// Bits in the R2400 Fault ID Register.
	enum faultid_bit {
		IBusMast2       = 15,
		IBusMast1       = 14,
		IBusMast0       = 13,
		IBusValidB      = 12,
		ProcBd          = 11,
		TimeOut         = 7,
		MReadQ          = 6,
		OldAccType1B    = 5,
		OldAccType0B    = 4,
		ParErr3B        = 3,
		ParErr2B        = 2,
		ParErr1B        = 1,
		ParErr0B        = 0,
	};

	// ID of board in this R2400 instance.
	enum r2400_cpuboard {
		cpuboard_m120	= 0x4,
		cpuboard_m180	= 0x9,
	};

	// Model of CPU card in this R2400 instance.
	enum r2400_model {
		model_unknown   = 0x00,
		model_m120_5    = 0x10,
		model_m120_3    = 0x20,
		model_rc3240    = 0x40,
	};

	// Trigger an address fault.
	void address_fault(u32 addr, bool writing);

	// accessors

	u8 duart0_r(offs_t offset);
	void duart0_w(offs_t offset, u8 data);

	u8 duart1_r(offs_t offset);
	void duart1_w(offs_t offset, u8 data);

	u8 rtc_r(offs_t offset);
	void rtc_w(offs_t offset, u8 data);

	u8 pit_r(offs_t offset);
	void pit_w(offs_t offset, u8 data);

	u16 syscfg_r(offs_t offset);
	void syscfg_w(offs_t offset, u16 data);

	void recalc_irq0();

	u16 isr_r(offs_t offset);
	void isr_w(offs_t offset, u16 data);

	u16 imr_r(offs_t offset);
	void imr_w(offs_t offset, u16 data);

	u32 far_r(offs_t offset);
	void far_w(offs_t offset, u32 data);

	u16 fid_r(offs_t offset);
	void fid_w(offs_t offset, u16 data);

	void led_w(offs_t offset, u8 data);

	u8 ethprom_r(offs_t offset);

	u8 idprom_r(offs_t offset);

	u16 am9516_ack_r(offs_t offset);
	void am9516_ack_w(offs_t offset, u16 data);

	u8 timer0_int_ack(offs_t offset);
	u8 timer1_int_ack_r(offs_t offset);
	void timer1_int_ack_w(offs_t offset, u8 data);

	u16 atbus_r(offs_t offset);
	void atbus_w(offs_t offset, u16 data);

	u16 atc_r(offs_t offset);
	void atc_w(offs_t offset, u16 data);

	void scsi_irq_w(int state);
	void scsi_drq_w(int state);

	u8 io_r(offs_t offset);
	void io_w(offs_t offset, u8 data);

	u8 huh_r(offs_t offset);
	void huh_w(offs_t offset, u8 data);

private:
	// processors and memory
	required_device<mips1_device_base> m_cpu;
	required_device<ram_device> m_ram;
	required_region_ptr<u32> m_rom;

	// I/O devices
	required_device_array<scn2681_device, 2> m_duart;
	required_device<input_merger_device> m_duart_irqs;
	required_device_array<rs232_port_device, 4> m_sio;
	required_device<nscsi_bus_device> m_scsibus;
	required_device<mb87030_device> m_scsi;
	required_device<am7990_device> m_net;
	required_device<m48t02_device> m_rtc;
	required_device<pit8254_device> m_pit;
	required_device<am9516_device> m_dma;

	// machine state
	r2400_cpuboard m_cpuboard;
	r2400_model m_model;
	u16 m_syscfg;
	u16 m_isr;
	u16 m_imr;
	u32 m_far;
	u16 m_fid;
	u8 m_led;
	int m_timer0_int;
	int m_timer1_int;
	u16 m_atc;
};

void mips_r2400_state::machine_start()
{
	fprintf(stderr, "%s\n", __func__);

	save_item(NAME(m_syscfg));
	save_item(NAME(m_isr));
	save_item(NAME(m_imr));
	save_item(NAME(m_far));
	save_item(NAME(m_fid));
	save_item(NAME(m_led));
}

void mips_r2400_state::machine_reset()
{
	fprintf(stderr, "%s\n", __func__);

	m_pit->write_gate0(ASSERT_LINE);
	m_pit->write_gate1(ASSERT_LINE);
	m_pit->write_gate2(ASSERT_LINE);
}

void mips_r2400_state::r2400_init()
{
	fprintf(stderr, "%s\n", __func__);

	// Map configured RAM

// 	m_cpu->space(0).install_ram(0x00000000, m_ram->mask(), m_ram->pointer());

	// Ensure the first page of the Boot PROM is present for startup.

	m_cpu->space(0).install_rom(0x1f000000, 0x1f000fff, m_ram->pointer());
}

static DEVICE_INPUT_DEFAULTS_START( terminal )
	DEVICE_INPUT_DEFAULTS( "RS232_RXBAUD", 0xff, RS232_BAUD_9600 )
	DEVICE_INPUT_DEFAULTS( "RS232_TXBAUD", 0xff, RS232_BAUD_9600 )
	DEVICE_INPUT_DEFAULTS( "RS232_DATABITS", 0xff, RS232_DATABITS_8 )
	DEVICE_INPUT_DEFAULTS( "RS232_PARITY", 0xff, RS232_PARITY_NONE )
	DEVICE_INPUT_DEFAULTS( "RS232_STOPBITS", 0xff, RS232_STOPBITS_1 )
DEVICE_INPUT_DEFAULTS_END

void mips_r2400_state::r2400(machine_config &config)
{
	fprintf(stderr, "%s\n", __func__);

	// CPU is set up by caller.

	// Memory

	RAM(config, m_ram);
	m_ram->set_default_size("48M");
	m_ram->set_extra_options("8M,16M,24M,32M,40M,48M");
	m_ram->set_default_value(0);

	// Serial

	SCN2681(config, m_duart[0], 3.6864_MHz_XTAL);
	SCN2681(config, m_duart[1], 3.6864_MHz_XTAL);

	INPUT_MERGER_ANY_HIGH(config, m_duart_irqs).output_handler().set_inputline(m_cpu, INPUT_LINE_IRQ1);
	m_duart[0]->irq_cb().set(m_duart_irqs, FUNC(input_merger_device::in_w<0>));
	m_duart[1]->irq_cb().set(m_duart_irqs, FUNC(input_merger_device::in_w<1>));

	RS232_PORT(config, m_sio[0], default_rs232_devices, "terminal");
	RS232_PORT(config, m_sio[1], default_rs232_devices, nullptr);
	RS232_PORT(config, m_sio[2], default_rs232_devices, nullptr);
	RS232_PORT(config, m_sio[3], default_rs232_devices, nullptr);

	m_duart[0]->a_tx_cb().set(m_sio[0], FUNC(rs232_port_device::write_txd));
	m_sio[0]->rxd_handler().set(m_duart[0], FUNC(scn2681_device::rx_a_w));
	// no CTS/DTR/RTS for SIO0
// 	m_sio[0]->cts_handler().set(m_sio[0], FUNC(rs232_port_device::write_rts));
// 	m_sio[0]->dsr_handler().set(m_sio[0], FUNC(rs232_port_device::write_dtr));
	m_duart[0]->b_tx_cb().set(m_sio[1], FUNC(rs232_port_device::write_txd));
	m_sio[1]->rxd_handler().set(m_duart[0], FUNC(scn2681_device::rx_b_w));
	m_sio[1]->cts_handler().set(m_duart[0], FUNC(scn2681_device::ip2_w));
	m_duart[0]->outport_cb().set(
		[this](u8 data) {
			m_sio[1]->write_dtr(BIT(data, 0));
			m_sio[1]->write_rts(BIT(data, 1));
		}
	);
	m_sio[0]->set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_sio[0]->set_option_device_input_defaults("terminal", DEVICE_INPUT_DEFAULTS_NAME(terminal));

	m_duart[1]->a_tx_cb().set(m_sio[2], FUNC(rs232_port_device::write_txd));
	m_sio[2]->rxd_handler().set(m_duart[1], FUNC(scn2681_device::rx_a_w));
	// no CTS/DTR/RTS for SIO2
// 	m_sio[2]->cts_handler().set(m_sio[2], FUNC(rs232_port_device::write_rts));
// 	m_sio[2]->dsr_handler().set(m_sio[2], FUNC(rs232_port_device::write_dtr));
	m_duart[1]->b_tx_cb().set(m_sio[3], FUNC(rs232_port_device::write_txd));
	m_sio[3]->rxd_handler().set(m_duart[1], FUNC(scn2681_device::rx_b_w));
	m_sio[3]->cts_handler().set(m_duart[1], FUNC(scn2681_device::ip2_w));
	m_duart[1]->outport_cb().set(
		[this](u8 data) {
			m_sio[3]->write_dtr(BIT(data, 0));
			m_sio[3]->write_rts(BIT(data, 1));
		}
	);

	// SCSI

	NSCSI_BUS(config, m_scsibus);
	NSCSI_CONNECTOR(config, "scsi:0", default_scsi_devices, "harddisk");
	NSCSI_CONNECTOR(config, "scsi:1", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:2", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:3", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:4", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:5", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:6", default_scsi_devices, nullptr);

	NSCSI_CONNECTOR(config, "scsi:7").option_set("mb87030", MB87030).machine_config(
		[](device_t *device)
		{
			mb87030_device &spc = downcast<mb87030_device &>(*device);

			spc.set_clock(16_MHz_XTAL / 2);
			spc.out_irq_callback().set(FUNC(mips_r2400_state::scsi_irq_w));
			spc.out_dreq_callback().set(FUNC(mips_r2400_state::scsi_drq_w));
		}
	);

	// Ethernet

	AM7990(config, m_net);

	// RTC

	M48T02(config, m_rtc);

	// PIT

	PIT8254(config, m_pit);
	// TODO: Set up gates in start?
	m_pit->set_clk<2>(3.6864_MHz_XTAL);
	m_pit->out_handler<0>().set(
		[this](int state) {
			if (state && (m_timer0_int == 0)) {
				m_timer0_int = state;
				m_cpu->set_input_line(INPUT_LINE_IRQ2, ASSERT_LINE);
			}
		}
	);
	m_pit->out_handler<1>().set(
		[this](int state) {
			if (state && (m_timer1_int == 0)) {
				m_timer1_int = state;
				m_cpu->set_input_line(INPUT_LINE_IRQ4, ASSERT_LINE);
			}
		}
	);
	m_pit->out_handler<2>().set(
		[this](int state) {
			m_pit->write_clk0(state);
			m_pit->write_clk1(state);
		}
	);

	// DMA

	AM9516(config, m_dma, 16_MHz_XTAL / 2);

	// PC/AT Bus

	// TODO: Add PC/AT Bus

	// Set up the registers.

	m_syscfg = (1 << Key0) | (1 << BootLockB) | (1 << ColdStart);

	// Set up the address map.

	m_cpu->set_addrmap(AS_PROGRAM, &mips_r2400_state::r2400_map);
}

void mips_r2400_state::m120_5(machine_config &config)
{
	fprintf(stderr, "%s\n", __func__);

	m_cpuboard = cpuboard_m120;
	m_model = model_m120_5;

	R2000A(config, m_cpu, 33.333_MHz_XTAL / 2, 32768, 32768);
	m_cpu->set_fpu(mips1_device_base::MIPS_R2010A);
	m_cpu->set_endianness(ENDIANNESS_BIG);
	m_cpu->in_brcond<0>().set_constant(1); // writeback complete
	m_cpu->set_addrmap(AS_PROGRAM, &mips_r2400_state::r2400_map);

	r2400(config);
}

void mips_r2400_state::m120_3(machine_config &config)
{
	fprintf(stderr, "%s\n", __func__);

	m_cpuboard = cpuboard_m120;
	m_model = model_m120_3;

	R2000A(config, m_cpu, 25_MHz_XTAL / 2, 32768, 32768);
	m_cpu->set_fpu(mips1_device_base::MIPS_R2010A);
	m_cpu->set_endianness(ENDIANNESS_BIG);
	m_cpu->in_brcond<0>().set_constant(1); // writeback complete
	m_cpu->set_addrmap(AS_PROGRAM, &mips_r2400_state::r2400_map);

	r2400(config);
}

void mips_r2400_state::rc3240(machine_config &config)
{
	fprintf(stderr, "%s\n", __func__);

	m_cpuboard = cpuboard_m180;
	m_model = model_rc3240;

	R3000(config, m_cpu, 50_MHz_XTAL / 2, 32768, 32768);
	m_cpu->set_fpu(mips1_device_base::MIPS_R3010);
	m_cpu->set_endianness(ENDIANNESS_BIG);
	m_cpu->in_brcond<0>().set_constant(1); // writeback complete
	m_cpu->set_addrmap(AS_PROGRAM, &mips_r2400_state::r2400_map);

	r2400(config);
}

void mips_r2400_state::r2400_map(address_map &map)
{
	fprintf(stderr, "%s\n", __func__);

//	map(0x00000000, 0x02ffffff).noprw(); // silence ram
	map(0x00000000, 0x02ffffff).ram().share("ram");

	map(0x10000000, 0x17ffffff).rw(FUNC(mips_r2400_state::atbus_r), FUNC(mips_r2400_state::atbus_w));

#if 0
	map(0x03000000, 0x1effffff).rw(FUNC(mips_r2400_state::io_r), FUNC(mips_r2400_state::io_w));
#else
	map(0x18000002, 0x18000003).rw(FUNC(mips_r2400_state::syscfg_r), FUNC(mips_r2400_state::syscfg_w));
	map(0x18010000, 0x18010003).rw(FUNC(mips_r2400_state::isr_r), FUNC(mips_r2400_state::isr_w)).umask32(0x0000ffff);
	map(0x18020000, 0x18020003).rw(FUNC(mips_r2400_state::imr_r), FUNC(mips_r2400_state::imr_w)).umask32(0x0000ffff);
	map(0x18030000, 0x18030003).rw(FUNC(mips_r2400_state::far_r), FUNC(mips_r2400_state::far_w));
	map(0x18040002, 0x18040003).rw(FUNC(mips_r2400_state::fid_r), FUNC(mips_r2400_state::fid_w));
	map(0x18050003, 0x18050003).r(FUNC(mips_r2400_state::timer0_int_ack));
	map(0x18060000, 0x180600ff).rw(FUNC(mips_r2400_state::timer1_int_ack_r), FUNC(mips_r2400_state::timer1_int_ack_w));
	map(0x18070000, 0x18070003).rw(FUNC(mips_r2400_state::atc_r), FUNC(mips_r2400_state::atc_w)).umask32(0x0000ffff);
	map(0x18080000, 0x18080003).w(FUNC(mips_r2400_state::led_w));
	map(0x18090000, 0x1809003f).rw(FUNC(mips_r2400_state::duart0_r), FUNC(mips_r2400_state::duart0_w));
	map(0x180a0000, 0x180a003f).rw(FUNC(mips_r2400_state::duart1_r), FUNC(mips_r2400_state::duart1_w));
	map(0x180b0000, 0x180b1fff).rw(FUNC(mips_r2400_state::rtc_r), FUNC(mips_r2400_state::rtc_w));
	map(0x180c0000, 0x180c000f).rw(FUNC(mips_r2400_state::pit_r), FUNC(mips_r2400_state::pit_w));
	map(0x180d0000, 0x180d00ff).m(m_scsi, FUNC(mb87030_device::map)).umask32(0x000000ff);
	map(0x180e0000, 0x180e0003).rw(m_dma, FUNC(am9516_device::addr_r), FUNC(am9516_device::addr_w)).umask32(0x0000ffff);
	map(0x180e0004, 0x180e0007).rw(m_dma, FUNC(am9516_device::data_r), FUNC(am9516_device::data_w)).umask32(0x0000ffff);
	map(0x180e0008, 0x180e000b).rw(FUNC(mips_r2400_state::am9516_ack_r), FUNC(mips_r2400_state::am9516_ack_w)).umask32(0x0000ffff);
	map(0x180f0000, 0x180f0007).rw(m_net, FUNC(am7990_device::regs_r), FUNC(am7990_device::regs_w)).umask32(0x0000ffff);

	map(0x1d000000, 0x1dffffff).r(FUNC(mips_r2400_state::ethprom_r));
	map(0x1e000000, 0x1effffff).r(FUNC(mips_r2400_state::idprom_r));
#endif

	map(0x1f000000, 0x1f03ffff).rom().region("r2400", 0);
	map(0x1fc00000, 0x1fc3ffff).rom().region("r2400", 0); // mirror
	map(0x1ff00000, 0x1ff3ffff).rom().region("r2400", 0); // mirror
}

void mips_r2400_state::address_fault(u32 addr, bool writing)
{
	fprintf(stderr, "address fault: 0x%08x (%s)\n", addr, writing ? "write" : "read");

	m_far = addr;

	m_fid  = (1 << ParErr3B);
	m_fid |= (1 << ParErr2B);
	m_fid |= (1 << ParErr1B);
	m_fid |= (1 << ParErr0B);
	m_fid |= (1 << IBusValidB);

	m_fid |= (1 << TimeOut);
	m_fid |= writing ? (1 << MReadQ) : (1 << ProcBd);
	m_fid |= (1 << IBusValidB);

	m_cpu->set_input_line(INPUT_LINE_IRQ5, ASSERT_LINE);
}

u16 mips_r2400_state::syscfg_r(offs_t offset)
{
	return m_syscfg;
}

void mips_r2400_state::syscfg_w(offs_t offset, u16 data)
{
	fprintf(stderr, "%s(0x%04x)\n", __func__, data);

	m_syscfg |= (data & 0x00ff);

	fprintf(stderr, "CFG: 0x%04x\n", m_syscfg);
}

void mips_r2400_state::recalc_irq0()
{
	if (m_isr & m_imr) {
		m_cpu->set_input_line(INPUT_LINE_IRQ0, ASSERT_LINE);
	} else {
		// Reading a device's ISR is supposed to be what deasserts IRQ0,
		// by having it clear its own IRQ which will in turn clear its
		// bit in the ISR.

		m_cpu->set_input_line(INPUT_LINE_IRQ0, CLEAR_LINE);
	}
}

u16 mips_r2400_state::isr_r(offs_t offset)
{
	fprintf(stderr, "%s(0x%08x) -> 0x%04x\n", __func__, offset, m_isr);

	// "The captured fault address is held until software reads the Interrupt
	// Status Register (ISR). Reading the ISR causes the Intr5* signal to be
	// de-asserted and also allows the FAR to resume latching physical
	// addresses."

	m_far = 0;
	m_cpu->set_input_line(INPUT_LINE_IRQ5, CLEAR_LINE);

	return m_isr;
}

void mips_r2400_state::isr_w(offs_t offset, u16 data)
{
	fprintf(stderr, "%s(0x%08x) -> 0x%04x\n", __func__, offset, data);

	m_isr = data;

	recalc_irq0();
}

u16 mips_r2400_state::imr_r(offs_t offset)
{
	fprintf(stderr, "%s(0x%08x) -> 0x%04x\n", __func__, offset, m_imr);

	return m_imr;
}

void mips_r2400_state::imr_w(offs_t offset, u16 data)
{
	fprintf(stderr, "%s(0x%08x) -> 0x%04x\n", __func__, offset, data);

	m_imr = data;
}

u32 mips_r2400_state::far_r(offs_t offset)
{
	// "This information is preserved in the FID register until the FAR is
	// read." So reset m_fid to the baseline values.

	m_fid  = (1 << ParErr3B);
	m_fid |= (1 << ParErr2B);
	m_fid |= (1 << ParErr1B);
	m_fid |= (1 << ParErr0B);
	m_fid |= (1 << IBusValidB);

	return m_far;
}

void mips_r2400_state::far_w(offs_t offset, u32 data)
{
	fprintf(stderr, "%s(0x%08x) -> 0x%08x\n", __func__, offset, data);

	m_far = data;
}

u16 mips_r2400_state::fid_r(offs_t offset)
{
	fprintf(stderr, "%s(0x%08x) -> 0x%04x\n", __func__, offset, m_fid);

	return m_fid;
}

void mips_r2400_state::fid_w(offs_t offset, u16 data)
{
	fprintf(stderr, "%s(0x%08x) -> 0x%04x\n", __func__, offset, data);

	m_fid = data;

	m_fid |= (1 << ParErr3B);
	m_fid |= (1 << ParErr2B);
	m_fid |= (1 << ParErr1B);
	m_fid |= (1 << ParErr0B);
	m_fid |= (1 << IBusValidB);
}

void mips_r2400_state::led_w(offs_t offset, u8 data)
{
	m_led = data;

	fprintf(stderr, "LED: ");
	fprintf(stderr, "%c", m_led & (1 << 7) ? '.' : '*');
	fprintf(stderr, "%c", m_led & (1 << 6) ? '.' : '*');
	fprintf(stderr, "%c", m_led & (1 << 5) ? '.' : '*');
	fprintf(stderr, "%c", m_led & (1 << 4) ? '.' : '*');
	fprintf(stderr, "%c", m_led & (1 << 3) ? '.' : '*');
	fprintf(stderr, "%c", m_led & (1 << 2) ? '.' : '*');
	fprintf(stderr, "%c", m_led & (1 << 1) ? '.' : '*');
	fprintf(stderr, "%c", m_led & (1 << 0) ? '.' : '*');
	fprintf(stderr, "\n");
}

u8 mips_r2400_state::ethprom_r(offs_t offset)
{
	fprintf(stderr, "%s(0x%08llx)\n", __func__, (long long)offset);

	return 0xff;
}

u8 mips_r2400_state::idprom_r(offs_t offset)
{
	static const char sn[6] = "12345";

	u8 data;

	switch (offset & 0x1f) {
		case 0x03:  data = m_cpuboard; break;
		case 0x07:  data = m_model; break;
		case 0x0b:  data = sn[0]; break;
		case 0x0f:  data = sn[1]; break;
		case 0x13:  data = sn[2]; break;
		case 0x17:  data = sn[3]; break;
		case 0x1b:  data = sn[4]; break;
		default:    data = 0xff; break;
	}

	return data;
}

u16 mips_r2400_state::am9516_ack_r(offs_t offset)
{
	fprintf(stderr, "%s(0x%08llx)\n", __func__, (long long)offset);

	return m_dma->acknowledge();
}

void mips_r2400_state::am9516_ack_w(offs_t offset, u16 data)
{
	fprintf(stderr, "%s(0x%08llx, 0x%04x)\n", __func__, (long long)offset, data);

	(void)m_dma->acknowledge();
}

u8 mips_r2400_state::duart0_r(offs_t offset)
{
	u8 data = m_duart[0]->read(offset >> 2);

//	fprintf(stderr, "%s(0x%08x) -> 0x%02x\n", __func__, offset, data);

	return data;
}

void mips_r2400_state::duart0_w(offs_t offset, u8 data)
{
	fprintf(stderr, "%s(0x%08x, 0x%02x)\n", __func__, offset >> 2, data);

	if ((offset >> 2) == 0x3) {
		fprintf(stderr, "TERM[0]: %c\n", (char)data);
	} else  if ((offset >> 2) == 0xb) {
		fprintf(stderr, "TERM[1]: %c\n", (char)data);
	}

	m_duart[0]->write(offset >> 2, data);
}

u8 mips_r2400_state::duart1_r(offs_t offset)
{
	u8 data = m_duart[1]->read(offset >> 2);

//	fprintf(stderr, "%s(0x%08x) -> 0x%02x\n", __func__, offset, data);

	return data;
}

void mips_r2400_state::duart1_w(offs_t offset, u8 data)
{
	fprintf(stderr, "%s(0x%08x, 0x%02x)\n", __func__, offset, data);

	m_duart[1]->write(offset >> 2, data);
}

u8 mips_r2400_state::rtc_r(offs_t offset)
{
	u8 data = m_rtc->read(offset);

	fprintf(stderr, "%s(0x%08x) -> 0x%02x\n", __func__, offset, data);

	return data;
}

void mips_r2400_state::rtc_w(offs_t offset, u8 data)
{
	fprintf(stderr, "%s(0x%08x) -> 0x%02x\n", __func__, offset, data);

	m_rtc->write(offset, data);
}

u8 mips_r2400_state::pit_r(offs_t offset)
{
	u8 data = m_pit->read(offset >> 2);

	fprintf(stderr, "%s(0x%08x) -> 0x%02x\n", __func__, offset, data);

	return data;
}

void mips_r2400_state::pit_w(offs_t offset, u8 data)
{
	fprintf(stderr, "%s(0x%08x, 0x%02x)\n", __func__, offset, data);

	m_pit->write(offset >> 2, data);
}

u8 mips_r2400_state::timer0_int_ack(offs_t offset)
{
	fprintf(stderr, "%s\n", __func__);

	u8 data = (m_timer0_int == 0);
	m_timer0_int = 0;
	m_cpu->set_input_line(INPUT_LINE_IRQ2, CLEAR_LINE);

	return data;
}

u8 mips_r2400_state::timer1_int_ack_r(offs_t offset)
{
	fprintf(stderr, "%s(0x%08llx)\n", __func__, (long long)offset);

	u8 data = 0xff;

	if (offset == 0x3) {
		data = (m_timer1_int == 0);
		m_timer1_int = 0;
		m_cpu->set_input_line(INPUT_LINE_IRQ4, CLEAR_LINE);
//	} else {
//		fprintf(stderr, "bus error\n");
//		data = 0xff;
//		m_cpu->berr_w(ASSERT_LINE);
	}

	return data;
}

void mips_r2400_state::timer1_int_ack_w(offs_t offset, u8 data)
{
	fprintf(stderr, "%s(0x%08llx, 0x%02x)\n", __func__, (long long)offset, data);

	if (offset == 0x3) {
		m_timer1_int = 0;
		m_cpu->set_input_line(INPUT_LINE_IRQ4, CLEAR_LINE);
	} else {
		m_cpu->berr_w(ASSERT_LINE);
	}
}

u16 mips_r2400_state::atbus_r(offs_t offset)
{
	u16 data = 0xffff;

	fprintf(stderr, "atbus_r: 0x%08x = 0x%04x\n", offset, data);

// 	address_fault(0x10000000 + offset, 0);

	return data;
}

void mips_r2400_state::atbus_w(offs_t offset, u16 data)
{
	fprintf(stderr, "atbus_w: 0x%08x = 0x%04x\n", offset, data);

// 	address_fault(0x10000000 + offset, 1);
}

u16 mips_r2400_state::atc_r(offs_t offset)
{
	u16 data = m_atc;

	fprintf(stderr, "atbus_r: 0x%08x = 0x%04x\n", offset, data);

	return data;
}

void mips_r2400_state::atc_w(offs_t offset, u16 data)
{
	fprintf(stderr, "atbus_w: 0x%08x = 0x%04x\n", offset, data);

	m_atc = data;
}

void mips_r2400_state::scsi_irq_w(int state)
{
	if (state == ASSERT_LINE) {
		fprintf(stderr, "SCSI IRQ asserted\n");
		m_isr |= (1 << 13);
	} else {
		fprintf(stderr, "SCSI IRQ cleared\n");
		m_isr &= ~(1 << 13);
	}

	recalc_irq0();
}

void mips_r2400_state::scsi_drq_w(int state)
{
	// Do nothing for now.
}

u8 mips_r2400_state::io_r(offs_t offset)
{
	u8 data;
//  bool writing = false;

	if (((offset + 0x03000000) & 0xFFFFFFFE) == 0x18000002) {
		fprintf(stderr, "reading syscfg\n");
		u16 syscfg = syscfg_r(0);
		switch (offset & 0x1) {
			case 0x0:   data = (syscfg & 0xff00) >> 8; break;
			case 0x1:   data = (syscfg & 0x00ff) >> 0; break;
		}
	} else
	if (((offset + 0x03000000) & 0xFFFFFFFC) == 0x18030000) {
		fprintf(stderr, "reading FAR\n");
		u32 far = far_r(0);
		switch (offset & 0x3) {
			case 0x0:   data = (far & 0xff000000) >> 24; break;
			case 0x1:   data = (far & 0x00ff0000) >> 16; break;
			case 0x2:   data = (far & 0x0000ff00) >> 8; break;
			case 0x3:   data = (far & 0x000000ff) >> 0; break;
		}
	} else
	if (((offset + 0x03000000) & 0xFFFF0000) == 0x18060000) {
		fprintf(stderr, "triggering bus error\n");
		m_cpu->berr_w(ASSERT_LINE);
		data = 0xff;
	} else
	if (((offset + 0x03000000) & 0xFFFF0000) == 0x1e000000) {
		data = idprom_r(offset & 0x1f);
//		address_fault(offset + 0x03000000, writing);
//		data = 0xff;
	} else
	{
		data = 0xff;
	}

	fprintf(stderr, "io_r: 0x%08x = 0x%02x\n", offset + 0x03000000, data);

	return data;
}

void mips_r2400_state::io_w(offs_t offset, u8 data)
{
	fprintf(stderr, "io_w: 0x%08x = 0x%02x\n", offset + 0x03000000, data);

	bool writing = true;
	if (((offset + 0x03000000) & 0xFFFF0000) == 0x18060000) {
		fprintf(stderr, "triggering bus error\n");
		m_cpu->berr_w(ASSERT_LINE);
	} else
	if (((offset + 0x03000000) & 0xFFFF0000) == 0x1e000000) {
		address_fault(offset + 0x03000000, writing);
	}

}

u8 mips_r2400_state::huh_r(offs_t offset)
{
	u8 data = 0xff;

	fprintf(stderr, "%s(0x%08llx, 0x%04x)\n", __func__, (long long)offset, data);

	return data;
}

void mips_r2400_state::huh_w(offs_t offset, u8 data)
{
	fprintf(stderr, "%s(0x%08llx, 0x%04x)\n", __func__, (long long)offset, data);
}


ROM_START(r2400)
	ROM_REGION32_BE(0x40000, "r2400", 0)

	ROM_SYSTEM_BIOS(0, "v5.10", "R2400 v5.10")
	ROMX_LOAD("50-00175-001.bin", 0x00000, 0x10000, CRC(82875dae) SHA1(de5ad8fca278dbb04f0ff03ebdf08bfe7032d601), ROM_SKIP(3) | ROM_BIOS(0))
	ROMX_LOAD("50-00172-001.bin", 0x00001, 0x10000, CRC(9a02f9c9) SHA1(c4a527acf51e521e3c8ceefca30d87602d8b1b9b), ROM_SKIP(3) | ROM_BIOS(0))
	ROMX_LOAD("50-00173-001.bin", 0x00002, 0x10000, CRC(59639df6) SHA1(918812cfbd50b3746344120157fec8d6a3215169), ROM_SKIP(3) | ROM_BIOS(0))
	ROMX_LOAD("50-00174-001.bin", 0x00003, 0x10000, CRC(02cfd4fe) SHA1(874408ab6342207682250dd275d8548607e86a22), ROM_SKIP(3) | ROM_BIOS(0))
ROM_END
#define rom_m120_5 rom_r2400
#define rom_m120_3 rom_r2400
#define rom_rc3240 rom_r2400

}

/*   YEAR   NAME       PARENT  COMPAT  MACHINE    INPUT  CLASS             INIT         COMPANY  FULLNAME  FLAGS */
COMP(1990,  m120_5,    0,      0,      m120_5,    0,     mips_r2400_state, r2400_init, "MIPS",  "M/120-5", MACHINE_NOT_WORKING | MACHINE_NO_SOUND)
COMP(1990,  m120_3,    0,      0,      m120_3,    0,     mips_r2400_state, r2400_init, "MIPS",  "M/120-3", MACHINE_NOT_WORKING | MACHINE_NO_SOUND)
COMP(1990,  rc3240,    0,      0,      rc3240,    0,     mips_r2400_state, r2400_init, "MIPS",  "RC3240",  MACHINE_NOT_WORKING | MACHINE_NO_SOUND)
