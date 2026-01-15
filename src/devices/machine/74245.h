// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/*****************************************************************************

    5/74245 Octal Bus Transceiver With 3-State Output

***********************************************************************

    Connection Diagram:
              ___ ___
      DIR   1 |*  u   | 20  Vcc
       A1   2 |*  u   | 19  /OE
       A2   3 |*  u   | 18  B1
       A3   4 |       | 17  B2
       A4   5 |       | 16  B3
       A5   6 |       | 15  B4
       A6   7 |       | 14  B5
       A7   8 |       | 13  B6
       A8   9 |       | 12  B7
      GND  10 |_______| 11  B8

*****************************************************************************/

#ifndef MAME_MACHINE_74245_H
#define MAME_MACHINE_74245_H

#pragma once

class ttl74245_device : public device_t
{
public:
	ttl74245_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto qa_cb() { return m_qa_func.bind(); }
	auto qb_cb() { return m_qb_func.bind(); }

	void a_w(u8 data) {
		m_aforb = data;

		do_output_if_necessary();
	}

	void b_w(u8 data) {
		m_bfora = data;

		do_output_if_necessary();
	}

	void dir_w(int level) {
		if (m_dir == level) return;
		m_dir = level;

		do_output_if_necessary();
	}

	void oe_w(int level) {
		if (m_oe == level) return;
		m_oe = level;

		do_output_if_necessary();
	}

protected:

	// device-level overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	/// Utility to invoke the output callback based on the selected
	/// direction but only if the output enable input is asserted.
	void do_output_if_necessary() {
		if (m_oe == 0) { // negative logic
			if (m_dir == 1) {
				if (!m_qb_func.isunset()) m_qb_func(m_aforb);
			} else if (m_dir == 0) {
				if (!m_qa_func.isunset()) m_qa_func(m_bfora);
			} else {
				// Unconfigured, do nothing.
			}
		}
	}

	devcb_write_line m_qa_func;
	devcb_write_line m_qb_func;

	u8 m_aforb, m_bfora;
	int m_dir, m_oe;
};

DECLARE_DEVICE_TYPE(TTL74245, ttl74245_device)

#endif /* MAME_MACHINE_74244_H */
