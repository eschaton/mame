// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

  HLE emulation of Profile & Widget

***************************************************************************/

#ifndef MAME_BUS_APPLEPP_STORAGEHLE_H
#define MAME_BUS_APPLEPP_STORAGEHLE_H

#pragma once

#include "applepp.h"

#include <queue>

class applepp_storage_device : public device_t, public device_applepp_interface
{
public:
	virtual void pd_w(u8 data) override;

	virtual void pstrb_w(int level) override;
	virtual void prw_w(int level) override;
	virtual void pcmd_w(int level) override;
	virtual void pres_w(int level) override;

protected:
	/// The commands a host may send to a storage device.
	typedef enum: u8 {
		read_command				= 0x00,
		write_command				= 0x01,
		write_verify_command		= 0x02,
		write_other_command			= 0x03,
		reset_command				= 0x80,
	} applepp_command_t;

	/// The next actions for the host in its storage device state machine.
	typedef enum: u8 {
		get_a_command				= 0x01,
		read_a_block				= 0x02,
		write_a_block				= 0x03,
		write_verify_a_block		= 0x04,
		write_actual_data			= 0x06,
	} applepp_next_action_t;

	/// The storage device's own state machine.
	typedef enum: int {
		IDLE = 0,
		READY_FOR_CMD,
		GETTING_CMD,
		START_READ_CMD,
		PERFORM_READ_CMD,
		START_WRITE_CMD,
		PERFORM_WRITE_CMD,
		ACK_WRITE_CMD,
		READ_WRITE_STATUS,
	} applepp_state_t;

	applepp_storage_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	void device_start() override;
	void device_reset() override;

	void read_block_from_image();
	void write_block_to_image();

	void indicate_presence();
	void assert_pbsy();
	void deassert_pbsy();
	void write_data_to_host(u8 data);

	void reset_state_machine();
	void run_state_machine();

	int m_strb, m_rw, m_cmd, m_res;
	u8 m_pd;

	applepp_command_t m_current_command;
	u8 m_block_address[3];
	int m_block_address_pos;
	u8 m_block[536];
	int m_block_pos;
	u8 m_current_status[4];
	u8 m_command_buf[6];
	int m_command_buf_idx;

	applepp_state_t m_state;
};

class applepp_profile_device : public applepp_storage_device
{
public:
	applepp_profile_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};

class applepp_widget_device : public applepp_storage_device
{
public:
	applepp_widget_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};


DECLARE_DEVICE_TYPE(APPLEPP_PROFILE, applepp_profile_device)
DECLARE_DEVICE_TYPE(APPLEPP_WIDGET,  applepp_widget_device)

#endif // MAME_BUS_APPLEPP_STORAGEHLE_H
