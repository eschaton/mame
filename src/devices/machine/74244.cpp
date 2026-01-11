// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/*****************************************************************************

    5/74244 Dual 4-bit Buffer/Driver

*****************************************************************************/

#include "emu.h"
#include "74244.h"

DEFINE_DEVICE_TYPE(TTL74244, ttl74244_device, "ttl74244", "54/74244 Dual 4-bit Buffer/Driver")

ttl74244_device::ttl74244_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, TTL74244, tag, owner, clock)
	, m_qa_cbs(*this)
	, m_qb_cbs(*this)
	, m_a(0)
	, m_b(0)
	, m_ga(0)
	, m_gb(0)
{}

void ttl74244_device::device_start()
{
	save_item(NAME(m_a));
	save_item(NAME(m_b));
	save_item(NAME(m_ga));
	save_item(NAME(m_gb));
}

void ttl74244_device::device_reset()
{
	m_a = m_b = 0;
	m_ga = m_gb = 0;
}
