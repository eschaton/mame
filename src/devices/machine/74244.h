// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/*****************************************************************************

    5/74244 Dual 4-bit Buffer/Driver

***********************************************************************

    Connection Diagram:
              ___ ___
      /GA   1 |*  u   | 20  Vcc
       A1   2 |*  u   | 19  /GB
      QB4   3 |*  u   | 18  QA1
       A2   4 |       | 17   B4
      QB3   5 |       | 16  QA2
       A3   6 |       | 15   B3
      QB2   7 |       | 14  QA3
       A4   8 |       | 13   B2
      QB1   9 |       | 12  QA4
      GND  10 |_______| 11   B1

*****************************************************************************/

#ifndef MAME_MACHINE_74244_H
#define MAME_MACHINE_74244_H

#pragma once

class ttl74244_device : public device_t
{
public:
	ttl74244_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	template <std::size_t Line> auto qa_cb() { return m_qa_cbs[Line].bind(); }
	template <std::size_t Line> auto qb_cb() { return m_qb_cbs[Line].bind(); }

	template <std::size_t Line>
	void a_w(int state) {
		int cur = (m_a & (1 << Line)) >> Line;
		if (cur == state) return;
		m_a &= ~(1 << Line);

		m_a |= (state << Line);
		if (!m_ga) {
			m_qa_cbs[Line](state);
		}
	}

	template <std::size_t Line>
	void b_w(int state) {
		int cur = (m_b & (1 << Line)) >> Line;
		if (cur == state) return;
		m_b &= ~(1 << Line);

		m_b |= (state << Line);
		if (!m_gb) {
			m_qb_cbs[Line](state);
		}
	}

	void ga_w(int state) {
		if (m_ga == state) return;
		m_ga = state;

		do_all_a_output_if_necessary();
	}

	void gb_w(int state) {
		if (m_gb == state) return;
		m_gb = state;

		do_all_b_output_if_necessary();
	}

protected:

	// device-level overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	void do_all_a_output_if_necessary() {
		if (!m_ga) { // negative logic
			m_qa_cbs[0](BIT(m_a, 0));
			m_qa_cbs[1](BIT(m_a, 1));
			m_qa_cbs[2](BIT(m_a, 2));
			m_qa_cbs[3](BIT(m_a, 3));
		}
	}

	void do_all_b_output_if_necessary() {
		if (!m_gb) { // negative logic
			m_qb_cbs[0](BIT(m_b, 0));
			m_qb_cbs[1](BIT(m_b, 1));
			m_qb_cbs[2](BIT(m_b, 2));
			m_qb_cbs[3](BIT(m_b, 3));
		}
	}

	devcb_write_line::array<4> m_qa_cbs;
	devcb_write_line::array<4> m_qb_cbs;

	u8 m_a, m_b;
	int m_ga, m_gb;
};

DECLARE_DEVICE_TYPE(TTL74244, ttl74244_device)

#endif /* MAME_MACHINE_74244_H */
