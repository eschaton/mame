// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/*********************************************************************

    profile.h

    Apple ProFile Hard Disk and its connector.

*********************************************************************/


#ifndef MAME_APPLE_PROFILE_H
#define MAME_APPLE_PROFILE_H

#pragma once


#include "emu.h"

#include <queue>


class profile_connector;


/// profile_device
///
/// The Apple ProFile Hard Disk Drive was the first hard disk sold by
/// Apple Computer, Inc. and was designed for use across its curent
/// product line, on Apple II, Apple III, and Lisa. It used a parallel
/// protocol from the system to the drive, where a custom Z8-based
/// interface board interacted with an ST-506 (5MB) or ST-412 (10MB)
/// hard disk mechanism to provide the actual storage.
class profile_device
	: public device_t
{
public:
	// Initialization
	profile_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// Host Connection
	void set_connector(profile_connector *connector);

	// Data Bus (from host->ProFile perspective)
	uint8_t read();
	void write(uint8_t data);

	// Control Signals (Write from Host)
	void res_w(int state);		// Reset
	void rrw_w(int state);		// Read/Write
	void pstrb_w(int state);	// Strobe
	void cmd_w(int state);		// Command

protected:
	// Initialization
	profile_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	// Common Device Functions
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

private:
	// Control Signals (Write from ProFile)
	void bsy_w(int state) { m_bsy_cb(state); }		// Busy

	// Control Signals (Callbacks to Host)
	auto bsy_cb() { return m_bsy_cb.bind(); }		// Busy

	// Connector
	profile_connector *m_connector;

	// Bus Signals
	devcb_write_line m_bsy_cb;		// Busy (ProFile->host)

	/// The direction for low-level byte read/write.
	enum byte_direction {
		host_write,
		host_read,
	} m_byte_direction;

	/// The states for low-level byte read/write.
	enum byte_state {
		idle = 0,
		host_will_read,
		host_did_read,
		host_will_write,
		host_did_write,
	} m_byte_state;
	void transition_byte_state(byte_state next);

	/// Bytes to send to the host. It's the host's responsibility to do
	/// the reads necessary (or assert CMD) to clear this queue.
	std::queue<uint8_t> m_to_host_bytes;

	/// All of the states an actual transaction can be in.
	enum transaction_state {
		/// Base idle state.
		awaiting_command = 0,

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
	} m_transaction_state;
	void update_transaction_state(transaction_state next);

	/// ProFile Commands
	enum profile_command: uint8_t {
		no_command = 0x00,
		read_command = 0x01,
		write_command = 0x02,
		write_verify_command = 0x03,
	};

	/// ProFile "Next Actions"
	enum profile_next_action: uint8_t {
		get_a_command = 0x01,
		read_a_block = 0x02,
		write_a_block = 0x03,
		write_verify_a_block = 0x04,
		write_actual_data = 0x06,
	};

	/// The command currently in flight.
	profile_command m_current_command;

	/// The address to which the command applies.
	uint8_t m_block_address[3];

	/// Counter for accumulating address.
	uint8_t m_block_address_counter;

	/// Retry count for read command.
	uint8_t m_retry_count;

	/// Sparing threshold for read command.
	uint8_t m_sparing_threshold;

	/// The statu of a ProFile command.
	union profile_status {
		uint8_t bytes[4];
		struct profile_status_s {
			// Byte 0
			unsigned no_ack : 1;
			unsigned write_abort : 1;
			unsigned host_data_flushed : 1;
			unsigned seek_error : 1;
			unsigned crc_error : 1;
			unsigned timeout_error : 1;
			unsigned unused_0_1 : 1;
			unsigned operation_failed : 1;

			// Byte 1
			unsigned seek_errorr : 1;
			unsigned spare_sector_table_overflow : 1;
			unsigned unused_1_5 : 1;
			unsigned bad_block_table_overflow : 1;
			unsigned status_sector_read_error : 1;
			unsigned used_spare : 1;
			unsigned seek_to_wrong_track_error : 1;
			unsigned unused_1_0 : 1;

			// Byte 2
			unsigned profile_has_been_reset : 1;
			unsigned invalid_block_number_error : 1;
			unsigned block_id_mismatch : 1;
			unsigned unused_2_4 : 1;
			unsigned unused_2_3 : 1;
			unsigned profile_was_reset : 1;
			unsigned profile_bad_response : 1;
			unsigned parity_error : 1;

			// Byte 3
			uint8_t read_error_count;
		} __attribute__((packed)) status;
	} m_current_status;

	/// Get the address to which the current command applies more conveniently.
	uint32_t block_address();

	/// The size of a ProFile block.
	static const size_t m_block_size = 532;

	/// The block that's currently in flight, if any.
	uint8_t m_block[m_block_size];

	/// Current position within the block in flight.
	size_t m_block_pos;

	/// Obtain the addressed block from the image.
	void read_block_from_image();

	/// Write the current block to the address in the image.
	void write_block_to_image();
};


/// profile_connector
///
/// A ProFile connector. This is split from the ProFile drive
/// implementation itself because there are distinct controllers with
/// different implementations for the Apple II, Apple III, and Lisa that
/// can all connect to a ProFile drive.
class profile_connector
	: public device_t
{
public:
	// Initialization
	profile_connector(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// ProFile connection
	void set_profile(profile_device *profile);

	// Data Bus (host<->ProFile)
	uint8_t host_r();
	void host_w(uint8_t data);

	// Data Bus (ProFile<->Host)
	uint8_t prof_r();
	void prof_w(uint8_t data);

	// Control Signals (Write from Host)
	void res_w(int state) { m_res_cb(state); }		// Reset
	void rrw_w(int state) { m_rrw_cb(state); };		// Read/Write
	void pstrb_w(int state) { m_pstrb_cb(state); }	// Strobe
	void cmd_w(int state) { m_cmd_cb(state); }		// Command

	// Control Signals (Write from ProFile)
	void bsy_w(int state) { m_bsy_cb(state); }		// Busy

	// Control Signals (Callbacks to Host)
	auto bsy_cb() { return m_bsy_cb.bind(); }		// Busy

	// Control Signals (Callbacks to ProFile)
	auto res_cb() { return m_res_cb.bind(); }		// Reset
	auto rrw_cb() { return m_rrw_cb.bind(); }		// Read/Write
	auto pstrb_cb() { return m_pstrb_cb.bind(); }	// Strobe
	auto cmd_cb() { return m_cmd_cb.bind(); }		// Command

protected:
	// Initialization
	profile_connector(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	// Common Device Functions
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

private:
	profile_device *m_profile;

	// Bus Signals
	uint8_t m_data;					// Data Bus (host<->ProFile)
	devcb_write_line m_res_cb;		// Reset (host->ProFile)
	devcb_write_line m_rrw_cb;		// Read/Write (host->ProFile)
	devcb_write_line m_pstrb_cb;	// Strobe (host->ProFile)
	devcb_write_line m_cmd_cb;		// Command (host->ProFile)
	devcb_write_line m_bsy_cb;		// Busy (ProFile->host)
};


DECLARE_DEVICE_TYPE(PROFILE, profile_device)
DECLARE_DEVICE_TYPE(PROFILE_CONNECTOR, profile_connector)


#endif // MAME_APPLE_PROFILE_H
