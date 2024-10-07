// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/*********************************************************************

    profile.cpp

    Apple ProFile Hard Disk and its connector.

	The protocol used by the ProFile is straightforward. Transactions
	are always under host control, and there are two layers of
	handshaking: One at the level of individual bytes using RRW and
	PSTRB, and one at the level of transactions using CMD and BSY.

	To read a byte from the bus:

	 1. The host sets RRW high to indicate it desires to read.
	 2. The host asserts PSTRB to tell the ProFile to write.
	 3. The ProFile writes the byte to the bus.
	 4. The host reads the byte from the bus.
	 5. The host deasserts PSTRB to indicate its read is complete.
	 5. More reads may be done, looping steps 2-5, without changing RRW.

	To write a byte to the bus:

	1. The host sets RRW low to indicate it desires to write.
	2. The host asserts PSTRB to tell the ProFile to expect a byte.
	3. The host writes a byte to the bus.
	4. The ProFile reads the byte from the bus.
	4. The host deasserts PSTRB to tell the ProFile its write is complete.
	5. More writes may be done, looping steps 2-5, without changing RRW.

	To perform a transaction:

	1. Host asserts CMD.
	2. ProFile writes the Next Action from its state machine to the bus.
	   a. 0x01 - Get a command
	   b. 0x02 - Read a block
	   c. 0x03 - Receive write data
	   d. 0x04 - Receive write/verify data
	   e. 0x06 - Perform write or write/verify
	3. ProFile asserts BSY, indicating it has a response available.
	4. Host reads Next Action from bus.
	5. Host writes OK (0x55) to bus.
	6. Host deasserts CMD, indicating an acknowledgment is available.
	7. ProFile deasserts BSY, indicating it has received acknowledgment.
	8. Host writes command bytes to the bus while ProFile reads them.
	9. ProFile advances its state machine based on command bytes.
	9. Host asserts CMD.
	10. ProFile writes Next Action from its state machine to bus.
	11. ProFile asserts BSY.
	12. Host reads Next Action from bus.
	12. Host writes OK (0x55) to bus.
	13. Host deasserts CMD.
	14. ProFile performs command.
	15. ProFile deasserts BSY.

	The rest depends on the specific command (read, write, write/verify).

	Commands start with a byte indicating the command: 0x00 for read,
	0x01 for write, and 0x02 for write/verify. Each command is followed
	by the three-byte block number it affects, with the most significant
	byte first. The read command follows the block number with a retry
	count byte and a sparing threshold byte, the write and write/verify
	commands do not.

	Device metadata is at blocks 0xFFFFFE and 0xFFFFFF.

*********************************************************************/

#include "emu.h"
#include "profile.h"

#include <assert.h>


#define VERBOSE  1
#define LOG_OUTPUT_FUNC printf
#include "logmacro.h"


/*********************************************************************
	ProFile
*********************************************************************/

/// A ProFile block specifier (address or count).
struct profile_block_specifier {
	uint8_t bytes[3];

	operator uint32_t() const {
		uint32_t specifier = 0x00;
		specifier |= ((uint32_t)bytes[0]) << 16;
		specifier |= ((uint32_t)bytes[1]) << 8;
		specifier |= ((uint32_t)bytes[2]) << 0;
		return specifier;
	}

	profile_block_specifier(uint32_t raw) {
		bytes[0] = (raw & 0x00FF0000) >> 16;
		bytes[1] = (raw & 0x0000FF00) >> 8;
		bytes[2] = (raw & 0x000000FF) >> 0;
	}
} __attribute__((packed));

/// The number of usable blocks in a 5MB ProFile.
static const uint32_t profile_block_count_5MB = 0x002600;

/// The number of usable bocks in a 10MB ProFile.
static const uint32_t profile_block_count_10MB = 0x004C00;

/// The ProFile spare table, which is really more of a device info
/// structure that includes the spare table. The size of the block
/// is the same as all others, 532 bytes.
union profile_spare_table {
	uint8_t bytes[532];
	struct profile_disk_info {
		uint8_t device_name[13] =  { 'P', 'R', 'O', 'F', 'I', 'L', 'E',
									 ' ', ' ', ' ', ' ', ' ', ' ' };
		uint8_t device_number[3] = { 0x00, 0x00, 0x00 };
		uint8_t program_revision[3] = { 0x04, 0x00 }; // call us "4.0"
		uint8_t blocks_available[3];
		uint8_t bytes_per_block[2] = { 0x02, 0x14 };
		uint8_t spare_sectors = 0x20;
		uint8_t spares_allocated = 0x00;
		uint8_t bad_blocks = 0x00;

		/// Spared and bad block lists
		/// - 3-byte block addresses
		/// - terminated with 0xff 0xff 0xff
		/// Since we don't actually need to worry about bad blocks,
		/// just set this entire region to 0xff.
		uint8_t spared_bad_block_list[505] = { 0xff };

		profile_disk_info(uint32_t profile_blocks = profile_block_count_5MB)
		{
			blocks_available[0] = (uint8_t)((profile_blocks & 0x00FF0000) >> 16);
			blocks_available[1] = (uint8_t)((profile_blocks & 0x0000FF00) >> 8);
			blocks_available[2] = (uint8_t)((profile_blocks & 0x000000FF) >> 8);
		}
	} __attribute__((packed)) disk_info;

	profile_spare_table(uint32_t block_count = profile_block_count_5MB)
		: disk_info(block_count)
	{}
};

const uint32_t profile_ram_buffer_address = 0x00fffffe;
const uint32_t profile_spare_table_address = 0x00ffffff;


// Initialization

/// Public initializer.
profile_device::profile_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: profile_device(mconfig, PROFILE, tag, owner, clock)
{
}

/// Shared initializer.
profile_device::profile_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, m_connector(nullptr)
	, m_bsy_cb(*this)
	, m_byte_direction(host_write)
	, m_byte_state(idle)
	, m_to_host_bytes()
	, m_transaction_state(awaiting_command)
	, m_current_command(no_command)
	, m_block_address()
	, m_block_address_counter(0)
	, m_retry_count(0)
	, m_sparing_threshold(0)
	, m_current_status()
	, m_block()
	, m_block_pos(0)
{
}

// Common Device Functions

void profile_device::device_start()
{
	LOG("profile_device::device_start\n");
}


void profile_device::device_reset()
{
	LOG("profile_device::device_reset\n");
}


void profile_device::device_add_mconfig(machine_config &config)
{
	LOG("profile_device::device_add_mconfig\n");
}

// Host Connection

void profile_device::set_connector(profile_connector *connector)
{
	LOG("profile_device::set_connector(%p)\n", connector);

	m_connector = connector;
	if (m_connector) {
		this->bsy_cb().set(*m_connector, FUNC(profile_connector::bsy_w));
	}
}

// Data Bus

uint8_t profile_device::read()
{
	LOG("profile_device::read()\n");
	assert(m_byte_direction == host_read);
	assert(m_byte_state == host_will_read);

	uint8_t data = m_to_host_bytes.front();
	m_to_host_bytes.pop();

	byte_state next = host_did_read;
	transition_byte_state(next);

	// Transaction state machine is entirely triggered by host writes and control signals.

	return data;
}

void profile_device::write(uint8_t data)
{
	LOG("profile_device::write(0x%02x)\n", data);
	assert(m_byte_direction == host_write);
	assert(m_byte_state == host_will_write);

	byte_state next = host_did_write;
	transition_byte_state(next);

	// Process relevant portions of our transaction state machine.

	switch (m_transaction_state) {
		case handshake_transaction: {
			if (data != 0x55) throw std::exception();
			update_transaction_state(finish_handshake);
		} break;

		case get_command: {
			m_current_command = (profile_command) data;
			update_transaction_state(get_address);
		} break;

		case get_address: {
			m_block_address[m_block_address_counter++] = data;

			// Once the block address is fully read, go to the next state.
			if (m_block_address_counter == 2) {
				m_block_address_counter = 0;

				// The next state depends on what command's in flight.
				switch (m_current_command) {
					case read_command:
						update_transaction_state(get_read_retry_count);
						break;

					case write_command:
						update_transaction_state(awaiting_write_goahead);
						break;

					case write_verify_command:
						update_transaction_state(awaiting_write_verify_goahead);
						break;

					default:
						assert(   (m_current_command == read_command)
						       || (m_current_command == write_command)
						       || (m_current_command == write_verify_command));
						break;
				}
			}
		} break;

		case get_read_retry_count: {
			m_retry_count = data;
			update_transaction_state(get_read_sparing_threshold);
		} break;

		case get_read_sparing_threshold: {
			m_sparing_threshold = data;
			update_transaction_state(awaiting_read_goahead);
			read_block_from_image();
		} break;

		case handshake_read_transaction: {
			if (data != 0x55) throw std::exception();
			update_transaction_state(finish_read_handshake);
		} break;

		case handshake_write_transaction: {
			if (data != 0x55) throw std::exception();
			update_transaction_state(finish_write_handshake);
		} break;

		case handshake_status_transaction: {
			if (data != 0x55) throw std::exception();
			update_transaction_state(finish_status_handshake);
		} break;

		case get_written_data: {
			assert(m_current_command == write);
			if (m_block_pos < m_block_size) {
				m_block[m_block_pos++] = data;
			} else {
				// just ignore too many bytes
			}
			// write and state change on next CMD assertion
		} break;

		default:
			assert(0);
			break;
	}
}

// Control Signals (Write from Host)

void profile_device::res_w(int state)
{
	LOG("profile_device::res_w(%d)\n", state);

	// Start byte state machine in idle, no read/write.
	m_byte_state = idle;
	m_byte_direction = host_write;
	m_transaction_state = awaiting_command;
	m_current_command = no_command;
	m_block_address[0] = 0x00;
	m_block_address[1] = 0x00;
	m_block_address[2] = 0x00;
	m_retry_count = 0x00;
	m_sparing_threshold = 0x00;
	m_block_pos = 0;
}

void profile_device::rrw_w(int state)
{
	LOG("profile_device::rrw_w(%d)\n", state);
	assert(m_byte_state == idle);

	// Change the expected direction of the next byte.
	// Don't change the byte state though, pstrb_w does that.
	m_byte_direction = state ? host_read : host_write;
}

void profile_device::pstrb_w(int state)
{
	LOG("profile_device::pstrb_w(%d)\n", state);
	if (state) {
		assert(m_byte_state == idle);
		byte_state next = m_byte_direction ? host_will_read : host_will_write;
		transition_byte_state(next);
	} else {
		assert(   ((m_byte_direction == host_read) && (m_byte_state == host_did_read))
		       || ((m_byte_direction == host_write) && (m_byte_state == host_did_write)));
		byte_state next = idle;
		transition_byte_state(next);
	}
}

void profile_device::cmd_w(int state)
{
	LOG("profile_device::cmd_w(%d)\n", state);

	if (state) {
		switch (m_transaction_state) {
			case handle_read_data: {
				// Clear queue of bytes headed to host.
				std::queue<uint8_t> empty;
				m_to_host_bytes.swap(empty);
				// otherwise behave like awaiting_command
			} //fallthrough

			case awaiting_command: {
				update_transaction_state(start_transaction);
				m_to_host_bytes.push(get_command);
				bsy_w(1);
				update_transaction_state(handshake_transaction);
			} break;

			case awaiting_read_goahead: {
				update_transaction_state(start_read_transaction);
				m_to_host_bytes.push(read_a_block);
				bsy_w(1);
				update_transaction_state(handshake_read_transaction);
			} break;

			case awaiting_write_goahead: {
				update_transaction_state(start_write_transaction);
				m_to_host_bytes.push(write_a_block);
				bsy_w(1);
				update_transaction_state(handshake_read_transaction);
			} break;

			case awaiting_write_verify_goahead: {
				update_transaction_state(start_write_verify_transaction);
				m_to_host_bytes.push(write_verify_a_block);
				bsy_w(1);
				update_transaction_state(handshake_write_transaction);
			} break;

			case get_written_data: {
				write_block_to_image();
				m_block_pos = 0;
				// otherwise behave like awaiting_status_goahead
			} //fallthrough

			case awaiting_status_goahead: {
				update_transaction_state(start_status_transaction);
				m_to_host_bytes.push(write_actual_data);
				bsy_w(1);
				update_transaction_state(handshake_status_transaction);
			} break;

			default:
				assert(0);
				break;
		}
	} else {
		switch (m_transaction_state) {
			case finish_handshake: {
				bsy_w(0);
				update_transaction_state(get_command);
			} break;

			case finish_read_handshake: {
				bsy_w(0);
				update_transaction_state(handle_read_status);
				m_to_host_bytes.push(m_current_status.bytes[0]);
				m_to_host_bytes.push(m_current_status.bytes[1]);
				m_to_host_bytes.push(m_current_status.bytes[2]);
				m_to_host_bytes.push(m_current_status.bytes[3]);
				update_transaction_state(handle_read_data);
				for (int i = 0; i < m_block_size; i++) {
					m_to_host_bytes.push(m_block[i]);
				}
				update_transaction_state(awaiting_command);
				// Done, back to the start!
			} break;

			case finish_write_handshake: {
				bsy_w(0);
				update_transaction_state(get_written_data);
			} break;

			case finish_status_handshake: {
				bsy_w(0);
				update_transaction_state(handle_write_status);
				m_to_host_bytes.push(m_current_status.bytes[0]);
				m_to_host_bytes.push(m_current_status.bytes[1]);
				m_to_host_bytes.push(m_current_status.bytes[2]);
				m_to_host_bytes.push(m_current_status.bytes[3]);
				update_transaction_state(awaiting_command);
				// Done, back to the start!
			} break;

			default:
				assert(0);
				break;
		}
	}
}

void profile_device::transition_byte_state(byte_state next)
{
	// Verify transition is as expected.
	//
	// The valid transitions are:
	//
	// - idle -> host_will_{read,write}
	//   - set by pstrb_w(1)
	//
	// - host_will_read -> host_did_read
	//   - set by read()
	//
	// - host_will_write -> host_did_write
	//   - set by write()
	//
	// - host_did_read, host_did_write -> idle
	//   - set by pstrb_w(0)
	//
	// Don't put functional code in this switch construct, it's just for
	// ensuring that we're going one from one valid state to another.
	byte_state previous = m_byte_state;
	switch (previous) {
		case idle: {
			assert(   ((m_byte_direction == host_read) && (next == host_will_read))
			       || ((m_byte_direction == host_write) && (next == host_will_write)));
		} break;

		case host_will_read: {
			assert(next == host_did_read);
		} break;

		case host_will_write: {
			assert(next == host_did_write);
		} break;

		case host_did_read: {
			assert(next == idle);
		} break;

		case host_did_write: {
			assert(next == idle);
		} break;
	}

	m_byte_state = next;
}

void profile_device::update_transaction_state(transaction_state next)
{
	transaction_state previous = m_transaction_state;
	m_transaction_state = next;

	switch (previous) {

		default:
			// Do nothing for now.
			break;
	}
}

/*********************************************************************
	Transaction state machine with state names.

	READ:
		awaiting_command:
			Host asserts CMD
		start_transaction:
			ProFile writes Next Action (0x01) to bus
			ProFile asserts BSY
		handshake_transaction:
			Host reads Next Action from bus
			Host writes 0x55 acknowledgment to bus
		finish_handshake:
			Host deasserts CMD
			ProFile deasserts BSY

		get_command:
			Host writes command byte to bus
			ProFile reads 1 command byte from bus
		get_address:
			Host writes address bytes to bus
			ProFile reads 3 address bytes from bus
		get_read_retry_count:
			Host writes retry count byte to bus
			ProFile reads retry count byte from bus
		get_read_sparing_threshold:
			Host writes sparing threshold byte to bus
			ProFile reads sparing threshold byte from bus

		awaiting_read_goahead:
			Host asserts CMD
		start_read_transaction:
			ProFile writes Next Action 0x02 to bus
			ProFile asserts BSY
		handshake_read_transaction:
			Host reads Next Action from bus
			Host writes 0x55 acknowledgment to bus
		finish_read_handshake:
			Host deasserts CMD
			ProFile deasserts BSY

		handle_read_status:
			ProFile writes 4 status bytes to bus
			Host reads 4 status bytes from bus

		handle_read_data:
			ProFile writes block bytes to bus
			If host asserts CMD, go to start_transaction

		awaiting_command:
			back to start!

	WRITE:
		awaiting_command:
			Host asserts CMD
		start_transaction:
			ProFile writes Next Action (0x01) to bus
			ProFile asserts BSY
		handshake_transaction:
			Host reads Next Action from bus
			Host writes 0x55 acknowledgment to bus
		finish_handshake:
			Host deasserts CMD
			ProFile deasserts BSY

		get_command:
			Host writes command bytes to bus
			ProFile reads 1 command byte from bus
		get_address:
			ProFile reads 3 address bytes from bus

		awaiting_write_goahead:
			Host asserts CMD
		start_write_transaction:
			ProFile writes Next Action 0x03 to bus
			ProFile asserts BSY
		handshake_write_transaction:
			Host reads Next Action from bus
			Host writes 0x55 acknowledgment to bus
			ProFile reads 0x55 acknowledgment from bus
		finish_write_handshake:
			Host deasserts CMD
			ProFile deasserts BSY

		get_written_data:
			Host writes addressed block contents to bus
			If host asserts CMD, go to awaiting_status_goahead

		awaiting_status_goahead:
			Host asserts CMD
		start_status_transaction:
			ProFile writes Next Action 0x06 to bus
			ProFile asserts BSY
		handshake_status_transaction:
			Host reads Next Action from bus
			Host writes 0x55 acknowledgment to bus
		finish_status_handshake:
			Host deasserts CMD
			ProFile deasserts BSY

		handle_write_status:
			ProFile writes 4 status bytes to bus
			Host reads 4 status bytes from bus

		awaiting_command:
			back to start!

	WRITE/VERIFY:
		Same as WRITE, except awaiting_write_goahead becomes awaiting_write_verify_goahead
		and start_write_transaction becomes start_write_verify_transaction which writes a
		Next Action of 0x04 instead of 0x03.
*********************************************************************/


uint32_t profile_device::block_address()
{
	uint32_t block_addr = 0x00;
	block_addr |= m_block_address[0] << 16;
	block_addr |= m_block_address[1] << 8;
	block_addr |= m_block_address[2] << 0;
	return block_addr;
}

void profile_device::read_block_from_image()
{
	uint32_t block_addr = block_address();
	LOG("profile_device::read_block_from_image(0x%6x)\n", block_addr);

	// TODO: Implement image support.
	// Just put in a recognizable pattern for now.
	for (int i = 0; i < m_block_size; i++) {
		m_block[i] = (uint8_t)(i % 0x100);
	}
}

void profile_device::write_block_to_image()
{
	uint32_t block_addr = block_address();
	LOG("profile_device::write_block_to_image(0x%6x)\n", block_addr);

	// TODO: Implement image support.
}


/*********************************************************************
	ProFile Connector
*********************************************************************/

// Initialization.

/// Public initializer.
profile_connector::profile_connector(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: profile_connector(mconfig, PROFILE_CONNECTOR, tag, owner, clock)
{
}

/// Shared initializer.
profile_connector::profile_connector(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, m_profile(nullptr)
	, m_data(0)
	, m_res_cb(*this)
	, m_rrw_cb(*this)
	, m_pstrb_cb(*this)
	, m_cmd_cb(*this)
	, m_bsy_cb(*this)
{
}

// Common Device Functions.

void profile_connector::device_start()
{
	LOG("profile_connector::device_start\n");
}

void profile_connector::device_reset()
{
	LOG("profile_connector::device_reset\n");
}

void profile_connector::device_add_mconfig(machine_config &config)
{
	LOG("profile_connector::device_add_mconfig\n");
}

void profile_connector::set_profile(profile_device *profile)
{
	LOG("profile_connector::set_profile(%p)\n", profile);

	// Disconnect from any already-connected ProFile device.
	if ((m_profile != nullptr) && (m_profile != profile)) {
		m_profile->set_connector(nullptr);
	}

	// Connect to the given ProFile device.

	m_profile = profile;
	if (m_profile) {
		// Set up our callbacks to talk to the ProFile.
		this->res_cb().set(*m_profile, FUNC(profile_device::res_w));
		this->rrw_cb().set(*m_profile, FUNC(profile_device::rrw_w));
		this->pstrb_cb().set(*m_profile, FUNC(profile_device::pstrb_w));
		this->cmd_cb().set(*m_profile, FUNC(profile_device::cmd_w));

		// Tell the ProFile to connect to us.
		m_profile->set_connector(this);
	}
}

uint8_t profile_connector::host_r()
{
	LOG("profile_connector::host_r");
	return m_data;
}

void profile_connector::host_w(uint8_t data)
{
	LOG("profile_connector::host_w(0x%02x)", data);
	m_data = data;
}

uint8_t profile_connector::prof_r()
{
	LOG("profile_connector::prof_r");
	return m_data;
}

void profile_connector::prof_w(uint8_t data)
{
	LOG("profile_connector::prof_w(0x%02x)", data);
	m_data = data;
}


/*********************************************************************
	Device Type Definitions
*********************************************************************/

DEFINE_DEVICE_TYPE(PROFILE, profile_device, "profile", "Apple ProFile Hard Disk")
DEFINE_DEVICE_TYPE(PROFILE_CONNECTOR, profile_connector, "profile_connector", "Apple ProFile Connector")
