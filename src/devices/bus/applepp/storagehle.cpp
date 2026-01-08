// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

#include "emu.h"
#include "storagehle.h"

#include <assert.h>

#include <iostream>

#define LOG_CTRL        (1U << 1)
#define LOG_DATA        (1U << 2)
#define LOG_BSTATE		(1U << 3)
#define LOG_TSTATE		(1U << 4)

#define LOGCTRL(...)    LOGMASKED(LOG_CTRL, "applepp_storage_device:CTRL: " __VA_ARGS__)
#define LOGDATA(...)    LOGMASKED(LOG_DATA, "applepp_storage_device:DATA: " __VA_ARGS__)
#define LOGBSTATE(...)	LOGMASKED(LOG_BSTATE, "applepp_stroage_device:BSTATE: " __VA_ARGS__)
#define LOGTSTATE(...)	LOGMASKED(LOG_TSTATE, "applepp_stroage_device:TSTATE: " __VA_ARGS__)

// #define VERBOSE (0)
#define VERBOSE (LOG_GENERAL|LOG_CTRL|LOG_DATA|LOG_TSTATE|LOG_TSTATE)
#define LOG_OUTPUT_STREAM std::cout
#include "logmacro.h"

DEFINE_DEVICE_TYPE(APPLEPP_PROFILE, applepp_profile_device, "profilehle", "Apple Profile drive HLE")
DEFINE_DEVICE_TYPE(APPLEPP_WIDGET,  applepp_widget_device,  "widgethle",  "Apple Widget drive HLE")


/**********************************************************************
	The protocol used by Apple's parallel port storage devices is
	straightforward, and common to ProFile and Widget. Third parties
	have also demonstrated that the various systems' implementations are
	fully general and not limited to the 5MB and 10MB device sizes
	produced by Apple.

	Transactions are always under host control, and there are two layers
	of handshaking: One at the level of individual bytes using RW and
	PSTRB, and one at the level of transactions using CMD and BSY.

	For the host to read a byte from the device:

	1. The host sets RW high to indicate it desires to read.
	2. The host asserts PSTRB to tell the device to write.
	3. The device writes the byte to the bus.
	4. The host reads the byte from the bus.
	5. The host deasserts PSTRB to indicate completion.
	5. More reads may be done, looping steps 2-5, without changing RW.

	For the host to write a byte to the device:

	1. The host sets RW low to indicate it desires to write.
	2. The host asserts PSTRB to tell the device to expect a byte.
	3. The host writes the byte to the bus.
	4. The device reads the byte from the bus.
	5. The host deasserts PSTRB to indicate completion.
	6. More writes may be done, looping steps 2-5, without changing RW.

	To perform a transaction:

	1. Host asserts CMD.
	2. Device writes the Next Action in its state machine to the bus,
	   which tells the host what to do, starting with the first:
	   a. 0x01 - Send a command
	   b. 0x02 - Receive a block from a read
	   c. 0x03 - Send data to be written
	   d. 0x04 - Send data to written and verified
	   e. 0x06 - Confirm the write or write/verify
	3. Device asserts BSY, indicating it has a response available.
	4. Host reads Next Action from bus.
	5. Host writes OK (0x55) to bus.
	6. Host deasserts CMD, indicating an acknowledgment is available.
	7. Device deasserts BSY, indicating it has received acknowledgment.
	8. Host writes command bytes to the bus while device reads them.
	9. Device advances its state machine based on command bytes.
	9. Host asserts CMD.
	10. Device writes Next Action from its state machine to bus.
	11. Device asserts BSY.
	12. Host reads Next Action from bus.
	12. Host writes OK (0x55) to bus.
	13. Host deasserts CMD.
	14. Device performs command.
	15. Device deasserts BSY.

	The rest depends on the specific command (read, write, or
	write/verify).

	Commands start with a byte indicating the command: 0x00 for read,
	0x01 for write, and 0x02 for write/verify. Each command is followed
	by the three-byte block number it affects, with the most significant
	byte first. The read command follows the block number with a retry
	count byte and a sparing threshold byte, while the write and
	write/verify commands do not.

	Device metadata is at blocks 0xFFFFFE and 0xFFFFFF.
*********************************************************************/


ALLOW_SAVE_TYPE(applepp_storage_device::applepp_command_t)
ALLOW_SAVE_TYPE(applepp_storage_device::applepp_direction_t)
ALLOW_SAVE_TYPE(applepp_storage_device::applepp_byte_state_t)
ALLOW_SAVE_TYPE(applepp_storage_device::applepp_transaction_state_t)
ALLOW_SAVE_TYPE(std::queue<u8>)


/* Device status byte 0 bit definitions. */
#define APPLEPP_STATUS_B0_NO_ACK						0x80
#define	APPLEPP_STATUS_B0_WRITE_ABORT					0x40
#define APPLEPP_STATUS_B0_HOST_DATA_FLUSHED				0x20
#define APPLEPP_STATUS_B0_SEEK_ERROR					0x10
#define APPLEPP_STATUS_B0_CRC_ERROR						0x08
#define APPLEPP_STATUS_B0_TIMEOUT_ERROR					0x04
#define APPLEPP_STATUS_B0_UNUSED_0_1					0x02
#define APPLEPP_STATUS_B0_OPERATION_FAILED				0x01

/* Device status byte 1 bit definitions. */
#define APPLEPP_STATUS_B1_SEEK_ERROR					0x80
#define APPLEPP_STATUS_B1_SPARE_SECTOR_TABLE_OVERFLOW	0x40
#define APPLEPP_STATUS_B1_UNUSED_1_5					0x20
#define APPLEPP_STATUS_B1_BAD_BLOCK_TABLE_OVERFLOW		0x10
#define APPLEPP_STATUS_B1_STATUS_SECTOR_READ_ERROR		0x08
#define APPLEPP_STATUS_B1_USED_SPARE					0x04
#define APPLEPP_STATUS_B1_SEEK_TO_WRONG_TRACK			0x02
#define APPLEPP_STATUS_B1_UNUSED_1_0					0x01

/* Device status byte 2 bit definitions. */
#define APPLEPP_STATUS_B2_DEVICE_HAS_BEEN_RESET			0x80
#define APPLEPP_STATUS_B2_INVALID_BLOCK_NUMBER			0x40
#define APPLEPP_STATUS_B2_BLOCK_ID_MISMATCH				0x20
#define APPLEPP_STATUS_B2_UNUSED_2_4					0x10
#define APPLEPP_STATUS_B2_UNUSED_2_3					0x08
#define APPLEPP_STATUS_B2_DEVICE_WAS_RESET				0x04
#define APPLEPP_STATUS_B2_DEVICE_BAD_RESPONSE			0x02
#define APPLEPP_STATUS_B2_PARITY_ERROR					0x01


/// The number of usable blocks in a 5MB ProFile.
static const u32 applepp_block_count_5MB = 0x002600;

/// The number of usable bocks in a 10MB ProFile or Widget.
static const u32 applepp_block_count_10MB = 0x004C00;

/// The Apple parallel storage device "spare table" is really more of a
/// "device info" structure that includes the spare table. The size of
/// the block is the same as all others, 532 bytes.
union applepp_spare_table {
	u8 bytes[532];

	struct applepp_storage_device_info {
		u8 device_name[13] =  { 'P', 'R', 'O', 'F', 'I', 'L', 'E',
								' ', ' ', ' ', ' ', ' ', ' ' };
		u8 device_number[3] = { 0x00, 0x00, 0x00 };
		u8 program_revision[3] = { 0x04, 0x00 }; // call us "4.0"
		u8 blocks_available[3];
		u8 bytes_per_block[2] = { 0x02, 0x14 };
		u8 spare_sectors = 0x20;
		u8 spares_allocated = 0x00;
		u8 bad_blocks = 0x00;

		/// Spared and bad block lists
		/// - 3-byte block addresses
		/// - terminated with 0xff 0xff 0xff
		/// Since we don't actually need to worry about bad blocks,
		/// just set this entire region to 0xff.
		u8 spared_bad_block_list[505] = { 0xff };

		applepp_storage_device_info(uint32_t blocks = applepp_block_count_5MB)
		{
			blocks_available[0] = (u8)((blocks & 0x00FF0000) >> 16);
			blocks_available[1] = (u8)((blocks & 0x0000FF00) >> 8);
			blocks_available[2] = (u8)((blocks & 0x000000FF) >> 8);
		}
	} __attribute__((packed)) device_info;

	applepp_spare_table(u32 block_count = applepp_block_count_5MB)
		: device_info(block_count)
	{}
};

/// The block address of the RAM buffer used by the device, which can be
/// accessed for diagnostic purposes.
const uint32_t applepp_ram_buffer_address = 0x00fffffe;

/// The block address of the spare table, which is really more of a
/// device info table that also happens to contain the table of spare
/// blocks. This is represented by the applepp_spare_table above.
const uint32_t applepp_spare_table_address = 0x00ffffff;


// Conveniences for dealing with negative logic.

#define assert_pbsy()	m_connector->pbsy_set(0)
#define deassert_pbsy()	m_connector->pbsy_set(1)


void applepp_storage_device::device_start()
{
	save_item(NAME(m_strb));
	save_item(NAME(m_rw));
	save_item(NAME(m_cmd));
	save_item(NAME(m_res));
	save_item(NAME(m_pd));

	save_item(NAME(m_byte_state));
	save_item(NAME(m_transaction_state));

	save_item(NAME(m_byte_direction));
	save_item(NAME(m_current_command));
	save_item(NAME(m_block_address));
	save_item(NAME(m_block_address_pos));
	save_item(NAME(m_retry_count));
	save_item(NAME(m_sparing_threshold));
	save_item(NAME(m_block));
	save_item(NAME(m_block_pos));
	save_item(NAME(m_current_status));
	save_item(NAME(m_to_host_bytes));

	m_strb = m_rw = m_cmd = m_res = 1;
	m_pd = 0;
}

void applepp_storage_device::device_reset()
{
	m_connector->pchk_set(0);
	m_connector->pbsy_set(1);
	m_connector->pd_set(0xff);

	m_byte_state = idle;
	m_transaction_state = awaiting_command;

	m_byte_direction = host_write;
	m_current_command = no_command;
	m_block_address[0] = 0;
	m_block_address[1] = 0;
	m_block_address[2] = 0;
	m_block_address_pos = 0;
	m_retry_count = 0;
	m_sparing_threshold = 0;
	memset(m_block, 0, 532);
	m_block_pos = 0;
	memset(m_current_status, 0, 4);

	std::queue<u8> empty;
	m_to_host_bytes.swap(empty);
}

void applepp_storage_device::pd_w(u8 data)
{
	m_pd = data;

	host_w(m_pd);
}

void applepp_storage_device::pstrb_w(int level)
{
	LOGCTRL("pstrb_w %02x (%s)\n", level, machine().describe_context());
	if(m_strb == level)
		return;
	m_strb = level;

	LOGCTRL("pstrb_w %02x (%s)\n", level, machine().describe_context());

	// When PSTRB is asserted, sample the read/write state and update
	// the byte state machine. When it's deasserted, go back to idle.

	if (!level) { // negative logic
		assert(m_byte_state == idle);
		applepp_byte_state_t next = m_byte_direction ? host_will_read : host_will_write;
		update_byte_state(next);

		// If the state is such that the host wants to read a byte, put
		// the next available byte on the bus. If there isn't any, put
		// 0x55 there to indicate an error.

		if (m_byte_state == host_will_read) {
			u8 data = host_r();

			m_connector->pd_set(data);
		}
	} else {
		assert(   ((m_byte_direction == host_read) && (m_byte_state == host_did_read))
		       || ((m_byte_direction == host_write) && (m_byte_state == host_did_write)));
		update_byte_state(idle);
	}
}

void applepp_storage_device::prw_w(int level)
{
	if(m_rw == level)
		return;
	m_rw = level;

	LOGCTRL("prw_w %02x (%s)\n", level, machine().describe_context());

	assert(m_byte_state == idle);

	// Change the expected direction of the next byte.
	// Don't change the byte state though, pstrb_w does that.
	m_byte_direction = !level ? host_write : host_read;
}

void applepp_storage_device::pcmd_w(int level)
{
	if(m_cmd == level)
		return;
	m_cmd = level;

	LOGCTRL("pcmd_w %02x (%s)\n", level, machine().describe_context());

	// Advance the state machines.

	if (!level) { // negative logic
		switch (m_transaction_state) {
			case handle_read_data: {
				// Clear queue of bytes headed to host.
				std::queue<u8> empty;
				m_to_host_bytes.swap(empty);
				// otherwise behave like awaiting_command
			} //fallthrough

			case awaiting_command: {
				update_transaction_state(start_transaction);
				m_to_host_bytes.push(get_command);
				assert_pbsy();
				update_transaction_state(handshake_transaction);
			} break;

			case awaiting_read_goahead: {
				update_transaction_state(start_read_transaction);
				m_to_host_bytes.push(get_command);
				assert_pbsy();
				update_transaction_state(handshake_read_transaction);
			} break;

			case awaiting_write_goahead: {
				update_transaction_state(start_write_transaction);
				m_to_host_bytes.push(get_command);
				assert_pbsy();
				update_transaction_state(handshake_read_transaction);
			} break;

			case awaiting_write_verify_goahead: {
				update_transaction_state(start_write_verify_transaction);
				m_to_host_bytes.push(get_command);
				assert_pbsy();
				update_transaction_state(handshake_write_transaction);
			} break;

			case get_written_data: {
				write_block_to_image();

				m_block_pos = 0;
				// otherwise behave like awaiting_status_goahead
			} //fallthrough

			case awaiting_status_goahead: {
				update_transaction_state(start_status_transaction);
				m_to_host_bytes.push(get_command);
				assert_pbsy();
				update_transaction_state(handshake_status_transaction);
			} break;

			default:
				assert(0);
				break;
		}
	} else {
		switch (m_transaction_state) {
			case finish_handshake: {
				deassert_pbsy();
				update_transaction_state(get_command);
			} break;

			case finish_read_handshake: {
				deassert_pbsy();

				update_transaction_state(handle_read_status);
				m_to_host_bytes.push(m_current_status[0]);
				m_to_host_bytes.push(m_current_status[1]);
				m_to_host_bytes.push(m_current_status[2]);
				m_to_host_bytes.push(m_current_status[3]);
				update_transaction_state(handle_read_data);
				for (int i = 0; i < 532; i++) {
					m_to_host_bytes.push(m_block[i]);
				}
				update_transaction_state(awaiting_command);
				// Done, back to the start!
			} break;

			case finish_write_handshake: {
				deassert_pbsy();
				update_transaction_state(get_written_data);
			} break;

			case finish_status_handshake: {
				deassert_pbsy();
				update_transaction_state(handle_write_status);
				m_to_host_bytes.push(m_current_status[0]);
				m_to_host_bytes.push(m_current_status[1]);
				m_to_host_bytes.push(m_current_status[2]);
				m_to_host_bytes.push(m_current_status[3]);
				update_transaction_state(awaiting_command);
				// Done, back to the start!
			} break;

			default:
				assert(0);
				break;
		}
	}
}

void applepp_storage_device::pres_w(int level)
{
	if(m_res == level)
		return;
	m_res = level;

	LOGCTRL("pres_w %02x (%s)\n", level, machine().describe_context());

	// Restart the byte and transaction state machines and clear buffers.

	m_byte_state = idle;
	m_transaction_state = awaiting_command;

	m_byte_direction = host_write;
	m_current_command = no_command;
	m_block_address[0] = 0x00;
	m_block_address[1] = 0x00;
	m_block_address[2] = 0x00;
	m_retry_count = 0x00;
	m_sparing_threshold = 0x00;
	memset(m_block, 0, 532);
	m_block_pos = 0;
	memset(m_current_status, 0, 4);
}

applepp_storage_device::applepp_storage_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock),
	  device_applepp_interface(mconfig, *this)
{
}

applepp_profile_device::applepp_profile_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: applepp_storage_device(mconfig, APPLEPP_PROFILE, tag, owner, clock)
{
}

applepp_widget_device::applepp_widget_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: applepp_storage_device(mconfig, APPLEPP_WIDGET, tag, owner, clock)
{
}

u8 applepp_storage_device::host_r()
{
	assert(m_byte_direction == host_read); // host must be reading
	assert(m_byte_state == host_will_read); // host must be expecting a read

	u8 data;
	if (m_to_host_bytes.size() > 0) {
		data = m_to_host_bytes.front();
		m_to_host_bytes.pop();
	} else {
		data = 0x55;
	}

	update_byte_state(host_did_read);

	LOGDATA("host_r %02x (%s)\n", data, machine().describe_context());

	// Transaction state machine is entirely triggered by host writes and control signals.

	return data;
}

void applepp_storage_device::host_w(u8 data)
{
	assert(m_rw == 1); // host must be writing
	assert(m_byte_state == host_will_write); // host must be expecting a write

	applepp_byte_state_t next = host_did_write;
	update_byte_state(next);

	LOGDATA("host_w %02x (%s)\n", data, machine().describe_context());

	// Process relevant portions of the transaction state machine.

	switch (m_transaction_state) {
		case handshake_transaction: {
			assert(data != 0x55);
			update_transaction_state(finish_handshake);
		} break;

		case get_command: {
			m_current_command = (applepp_command_t) data;
			update_transaction_state(get_address);
		} break;

		case get_address: {
			m_block_address[m_block_address_pos++] = data;

			// Once the block address is fully read, go to the next state.
			if (m_block_address_pos == 2) {
				m_block_address_pos = 0;

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

			// Read the specified block from the backing image.
			read_block_from_image();
		} break;

		case handshake_read_transaction: {
			assert(data != 0x55);
			update_transaction_state(finish_read_handshake);
		} break;

		case handshake_write_transaction: {
			assert(data != 0x55);
			update_transaction_state(finish_write_handshake);
		} break;

		case handshake_status_transaction: {
			assert(data != 0x55);
			update_transaction_state(finish_status_handshake);
		} break;

		case get_written_data: {
			assert(m_current_command == write);
			if (m_block_pos < 532) {
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

void applepp_storage_device::update_byte_state(applepp_byte_state_t next)
{
	/*
		Byte state machine with state names.

		The valid transitions are:

		- idle -> host_will_{read,write}
		  - set by pstrb_w(1)

		- host_will_read -> host_did_read
		  - set by host_r()

		- host_will_write -> host_did_write
		  - set by host_w()

		- host_did_read, host_did_write -> idle
		  - set by pstrb_w(0)

		Don't put any behavioral in this method, it's just for ensuring
		that we're going one from one valid state to another, and should
		reduce to a single assignment in a non-debug build.
	*/

	applepp_byte_state_t previous = m_byte_state;
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

void applepp_storage_device::update_transaction_state(applepp_transaction_state_t next)
{
	/*
		Transaction state machine with state names.

		The valid transitions for each command are:

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
			Same as WRITE, except awaiting_write_goahead becomes
			awaiting_write_verify_goahead and start_write_transaction
			becomes start_write_verify_transaction which writes a Next
			Action of 0x04 instead of 0x03.

		Don't put any behavioral in this method, it's just for ensuring
		that we're going one from one valid state to another, and should
		reduce to a single assignment in a non-debug build.
	*/

	// TODO: Implement transaction state validator.

	m_transaction_state = next;
}

void applepp_storage_device::read_block_from_image()
{
	const u32 address = (  (m_block_address[0] << 16)
						 | (m_block_address[1] << 8)
						 | (m_block_address[2]));

	// TODO: Get size from attached image.
	u32 size = applepp_block_count_5MB;

	if (address < size) {
		// TODO: Read the block from the attached image.
	} else if (address == applepp_spare_table_address) {
		applepp_spare_table info(size);
		memcpy(m_block, info.bytes, 532);
	} else if (address == applepp_ram_buffer_address) {
		// The HLE doen't have a RAM buffer to read/write.
		// So just return whatever is in the current block.
	} else  {
		// TODO: Generate an error, block out of range.
	}
}

void applepp_storage_device::write_block_to_image()
{
	const u32 address = (  (m_block_address[0] << 16)
						 | (m_block_address[1] << 8)
						 | (m_block_address[2]));

	// TODO: Get size from attached image.
	u32 size = applepp_block_count_5MB;

	if (address < size) {
		// TODO: Write the current block to the attached image.
	} else if (address == applepp_ram_buffer_address) {
		// The HLE doen't have a RAM buffer to read/write.
		// So do nothing.
	} else {
		// TODO: Generate an error, block out of range.
	}
}
