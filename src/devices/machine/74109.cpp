#include "emu.h"
#include "74109.h"

DEFINE_DEVICE_TYPE(TTL74109, ttl74109_device, "ttl74109", "54/74109 Dual J-/K Positive-Edge-Triggered Flip-Flops with Preset and Clear")




ttl74109_device::ttl74109_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, TTL74109, tag, owner, clock)
	, m_ff1_q_func(*this)
	, m_ff2_q_func(*this)
	, m_ff1()
	, m_ff2()
{
	m_ff1.set_q_cb([this](bool q) { this->m_ff1_q_func(q ? 1 : 0); });
	m_ff2.set_q_cb([this](bool q) { this->m_ff2_q_func(q ? 1 : 0); });
}

void ttl74109_device::device_start()
{
}

void ttl74109_device::device_reset()
{
}

// Flip-Flop 1

void ttl74109_device::ff1_pre_w(int state)
{
	m_ff1.pre_w(state != 0);
}

void ttl74109_device::ff1_clr_w(int state)
{
	m_ff1.clr_w(state != 0);
}

void ttl74109_device::ff1_clk_w(int state)
{
	m_ff1.clk_w(state != 0);
}

void ttl74109_device::ff1_j_w(int state)
{
	m_ff1.j_w(state != 0);
}

void ttl74109_device::ff1_k_w(int state)
{
	m_ff1.k_w(state != 0);
}

void ttl74109_device::ff1_q_w(bool state)
{
	m_ff1_q_func(state ? 1 : 0);
}

// Flip-Flop 2

void ttl74109_device::ff2_pre_w(int state)
{
	m_ff2.pre_w(state != 0);
}

void ttl74109_device::ff2_clr_w(int state)
{
	m_ff2.clr_w(state != 0);
}

void ttl74109_device::ff2_clk_w(int state)
{
	m_ff2.clk_w(state != 0);
}

void ttl74109_device::ff2_j_w(int state)
{
	m_ff2.j_w(state != 0);
}

void ttl74109_device::ff2_k_w(int state)
{
	m_ff2.k_w(state != 0);
}

void ttl74109_device::ff2_q_w(bool state)
{
	m_ff2_q_func(state ? 1 : 0);
}
