#ifndef MAME_MACHINE_74109_H
#define MAME_MACHINE_74109_H

#pragma once

#include "flip_flop_jk.h"


/// A ttl74109_device is a dual J/K flip-flop.
///
/// - NOTE: This device uses entirely positive logic, unlike the 74109
///         itself; an actual 74109 has active-low PRE, CLR, and K inputs,
///         and both active-high and active-low Q outputs.
class ttl74109_device : public device_t
{
public:
	ttl74109_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto ff1_q_callback() { return m_ff1_q_func.bind(); }

	void ff1_pre_w(int state);
	void ff1_clr_w(int state);
	void ff1_clk_w(int state);
	void ff1_j_w(int state);
	void ff1_k_w(int state);

	auto ff2_q_callback() { return m_ff2_q_func.bind(); }

	void ff2_pre_w(int state);
	void ff2_clr_w(int state);
	void ff2_clk_w(int state);
	void ff2_j_w(int state);
	void ff2_k_w(int state);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	devcb_write_line m_ff1_q_func;
	devcb_write_line m_ff2_q_func;

	flip_flop_jk m_ff1;
	flip_flop_jk m_ff2;

	void ff1_q_w(bool state);
	void ff2_q_w(bool state);
};


DECLARE_DEVICE_TYPE(TTL74109, ttl74109_device)

#endif /* MAME_MACHINE_74109_H */
