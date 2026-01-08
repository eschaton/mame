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
		no_command					= 0x00,
		read_command				= 0x01,
		write_command				= 0x02,
		write_verify_command		= 0x03,
	} applepp_command_t;

	/// The next actions for the host in the storage device state machine.
	typedef enum: u8 {
		get_a_command				= 0x01,
		read_a_block				= 0x02,
		write_a_block				= 0x03,
		write_verify_a_block		= 0x04,
		write_actual_data			= 0x06,
	} applepp_next_action_t;

	/// The direction for low-level byte read/write.
	typedef enum {
		host_write,
		host_read,
	} applepp_direction_t;

	/// The byte-level storage device state.
	typedef enum {
		idle,
		host_will_read,
		host_will_write,
		host_did_read,
		host_did_write,
	} applepp_byte_state_t;

	/// The transaction-level storage device state.
	typedef enum {
		/// Base idle state.
		awaiting_command,

		/// Starting command transaction.
		start_transaction,
		handshake_transaction,
		finish_handshake,

		/// Getting a command byte from the bus.
		get_command,

		/// Getting a block address from the bus.
		get_address,

		/// Getting retry count for read.
		get_read_retry_count,

		/// Getting sparing threshold for read.
		get_read_sparing_threshold,

		/// Ready to start actual read.
		awaiting_read_goahead,

		/// Starting actual read.
		start_read_transaction,
		handshake_read_transaction,
		finish_read_handshake,

		/// Sending status for read.
		handle_read_status,

		/// Sending data for read.
		handle_read_data,

		/// Ready to start actual write.
		awaiting_write_goahead,

		/// Starting actual write.
		start_write_transaction,
		handshake_write_transaction,
		finish_write_handshake,

		/// Getting written data.
		get_written_data,

		/// Ready to send back status.
		awaiting_status_goahead,

		/// Sending status for write or write/verify.
		start_status_transaction,
		handshake_status_transaction,
		finish_status_handshake,

		/// Sending status data.
		handle_write_status,

		/// Ready to start actual write/verify.
		awaiting_write_verify_goahead,

		/// Starting actual write/verify.
		start_write_verify_transaction,
	} applepp_transaction_state_t;

	applepp_storage_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	void device_start() override;
	void device_reset() override;

	u8 host_r();
	void host_w(u8 data);

	void update_byte_state(applepp_byte_state_t next);
	void update_transaction_state(applepp_transaction_state_t next);

	void read_block_from_image();
	void write_block_to_image();

	int m_strb, m_rw, m_cmd, m_res;
	u8 m_pd;

	applepp_byte_state_t m_byte_state;
	applepp_transaction_state_t m_transaction_state;

	applepp_direction_t m_byte_direction;
	applepp_command_t m_current_command;
	u8 m_block_address[3];
	int m_block_address_pos;
	u8 m_retry_count;
	u8 m_sparing_threshold;
	u8 m_block[532];
	int m_block_pos;
	u8 m_current_status[4];

	/// Bytes to send to the host. It's the host's responsibility to do
	/// the reads necessary (or assert CMD) to clear this queue.
	std::queue<u8> m_to_host_bytes;
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
