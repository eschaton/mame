// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/*****************************************************************************

    5/74245 Octal Bus Transceiver With 3-State Output

*****************************************************************************/

#include "emu.h"
#include "74245.h"

DEFINE_DEVICE_TYPE(TTL74245, ttl74245_device, "ttl74245", "54/74245 Octal Bus Transceiver With 3-State Output")

ttl74245_device::ttl74245_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, TTL74245, tag, owner, clock)
	, m_qa_func(*this)
	, m_qb_func(*this)
	, m_aforb(0)
	, m_bfora(0)
	, m_dir(0)
	, m_oe(0)
{}

void ttl74245_device::device_start()
{
	save_item(NAME(m_aforb));
	save_item(NAME(m_bfora));
	save_item(NAME(m_dir));
	save_item(NAME(m_oe));
}

void ttl74245_device::device_reset()
{
	m_aforb = m_bfora = 0;
	m_dir = m_oe = 0;
}
