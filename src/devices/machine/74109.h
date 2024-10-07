#ifndef MAME_MACHINE_74109_H
#define MAME_MACHINE_74109_H

#pragma once

#include "flip_flop_jk.h"

DECLARE_DEVICE_TYPE(TTL74109, ttl74109_device)

/// The 74109 is a TTL dual J/K edge-triggered flip-flop.
///
/// Note that several of the 74109 signals are active-low and represented
/// that way in this device in order to more directly match uses of the
/// 74109 without requiring additional translation from active-low to
/// active-high. (The underlying J/K flip-flops are however implemented
/// using positive logic.)
class ttl74109_device : public device_t
{
public:
	ttl74109_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0)
		: device_t(mconfig, TTL74109, tag, owner, clock)
		, m_ff1()
		, m_ff1_q_func(*this)
		, m_ff1_not_q_func(*this)
		, m_ff2()
		, m_ff2_q_func(*this)
		, m_ff2_not_q_func(*this)
	{
		m_ff1.set_q_cb([this](bool q) { this->m_ff1_q_func(q ? 1 : 0); });
		m_ff2.set_q_cb([this](bool q) { this->m_ff2_q_func(q ? 1 : 0); });
	}

	auto ff1_q_cb() { return m_ff1_q_func.bind(); }
	auto ff1_not_q_cb() { return m_ff1_not_q_func.bind(); }

	void ff1_not_pre_w(int state) { m_ff1.pre_w(state ? false : true); }
	void ff1_not_clr_w(int state) { m_ff1.clr_w(state ? false : true); }
	void ff1_clk_w(int state) { m_ff1.clk_w(state ? true : false); }
	void ff1_j_w(int state) { m_ff1.j_w(state ? true : false); }
	void ff1_not_k_w(int state) { m_ff1.k_w(state ? false : true); }

	auto ff2_q_cb() { return m_ff2_q_func.bind(); }
	auto ff2_not_q_cb() { return m_ff2_not_q_func.bind(); }

	void ff2_pre_w(int state);
	void ff2_clr_w(int state);
	void ff2_clk_w(int state);
	void ff2_j_w(int state);
	void ff2_k_w(int state);

protected:
	virtual void device_start() override ATTR_COLD {}
	virtual void device_reset() override ATTR_COLD {}

private:
	void ff1_q_w(bool state) {
		m_ff1_q_func(state ? 1 : 0);
		m_ff1_not_q_func(state ? 0 : 1);
	}

	void ff2_q_w(bool state) {
		m_ff2_q_func(state ? 1 : 0);
		m_ff2_not_q_func(state ? 0 : 1);
	}

	flip_flop_jk m_ff1;
	devcb_write_line m_ff1_q_func;
	devcb_write_line m_ff1_not_q_func;

	flip_flop_jk m_ff2;
	devcb_write_line m_ff2_q_func;
	devcb_write_line m_ff2_not_q_func;
};

#endif /* MAME_MACHINE_74109_H */
