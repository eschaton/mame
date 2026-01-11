// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

#include "emu.h"
#include "storagehle.h"

#include <assert.h>

#include <iostream>

#define LOG_CTRL        (1U << 1)
#define LOG_DATA        (1U << 2)
#define LOG_STATE       (1U << 3)

#define LOGCTRL(...)    LOGMASKED(LOG_CTRL, "applepp_storage_device:CTRL: " __VA_ARGS__)
#define LOGDATA(...)    LOGMASKED(LOG_DATA, "applepp_storage_device:DATA: " __VA_ARGS__)
#define LOGSTATE(...)   LOGMASKED(LOG_STATE, "applepp_stroage_device:STATE: " __VA_ARGS__)

// #define VERBOSE (0)
#define VERBOSE (LOG_GENERAL|LOG_CTRL|LOG_DATA|LOG_STATE)
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

    The device's RAM buffer is at block 0xFFFFFE, and the device
    information and spare table is at block 0xFFFFFF. In theory, a
    device can support up to 0xFFFFFE blocks of 512 usable bytes each,
    or just shy of 8GB. However, none of the filesystems used by systems
    that support ProFile support that large a volume, and none support
    partitioning of devices into multiple volumes.

    The low-level format firmware for ProFile says these are the commands
    supported by it, which diverges from other sources (including other
    firmware) starting at 02.

        00 READ
        01 WRITE
        02 SCAN ALL BLOCKS
        03 SEEK
        04 FORMAT
        05 INIT SPARE TABLE
        06 TEST CONTROLLER RAM
        07 READ HEADER
        08 ERASE TRACK
        09 WRITE SECTOR MARKS
        0A WRITE HEADERS
        0B TURN OFF STEPPER
        0C PARK HEADS
        0D GET RESULT TABLE

*********************************************************************/


ALLOW_SAVE_TYPE(applepp_storage_device::applepp_command_t)
ALLOW_SAVE_TYPE(applepp_storage_device::applepp_state_t)

/* Device status byte 0 bit definitions. */
#define APPLEPP_STATUS_B0_NO_ACK                        0x80
#define APPLEPP_STATUS_B0_WRITE_ABORT                   0x40
#define APPLEPP_STATUS_B0_HOST_DATA_FLUSHED             0x20
#define APPLEPP_STATUS_B0_SEEK_ERROR                    0x10
#define APPLEPP_STATUS_B0_CRC_ERROR                     0x08
#define APPLEPP_STATUS_B0_TIMEOUT_ERROR                 0x04
#define APPLEPP_STATUS_B0_UNUSED_0_1                    0x02
#define APPLEPP_STATUS_B0_OPERATION_FAILED              0x01

/* Device status byte 1 bit definitions. */
#define APPLEPP_STATUS_B1_SEEK_ERROR                    0x80
#define APPLEPP_STATUS_B1_SPARE_SECTOR_TABLE_OVERFLOW   0x40
#define APPLEPP_STATUS_B1_UNUSED_1_5                    0x20
#define APPLEPP_STATUS_B1_BAD_BLOCK_TABLE_OVERFLOW      0x10
#define APPLEPP_STATUS_B1_STATUS_SECTOR_READ_ERROR      0x08
#define APPLEPP_STATUS_B1_USED_SPARE                    0x04
#define APPLEPP_STATUS_B1_SEEK_TO_WRONG_TRACK           0x02
#define APPLEPP_STATUS_B1_UNUSED_1_0                    0x01

/* Device status byte 2 bit definitions. */
#define APPLEPP_STATUS_B2_DEVICE_HAS_BEEN_RESET         0x80
#define APPLEPP_STATUS_B2_INVALID_BLOCK_NUMBER          0x40
#define APPLEPP_STATUS_B2_BLOCK_ID_MISMATCH             0x20
#define APPLEPP_STATUS_B2_UNUSED_2_4                    0x10
#define APPLEPP_STATUS_B2_UNUSED_2_3                    0x08
#define APPLEPP_STATUS_B2_DEVICE_WAS_RESET              0x04
#define APPLEPP_STATUS_B2_DEVICE_BAD_RESPONSE           0x02
#define APPLEPP_STATUS_B2_PARITY_ERROR                  0x01


/// The number of usable blocks in a 5MB ProFile.
static const u32 applepp_block_count_5MB = 0x002600;

/// The number of usable bocks in a 10MB ProFile or Widget.
static const u32 applepp_block_count_10MB = 0x004C00;

/// The name of a ProFile drive.
static const char applepp_device_name_ProFile[14] = "PROFILE      ";

/// The name of a Widget drive.
static const char applepp_device_name_Widget[14]  = "WIDGET       ";


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
		u8 blocks_available[3] = { 0x00, 26, 00 }; // 5MB to start
		u8 bytes_per_block[2] = { 0x02, 0x14 }; // 532 = 0x0214
		u8 spare_sectors = 0x20;
		u8 spares_allocated = 0x00;
		u8 bad_blocks = 0x00;

		/// Spared and bad block lists
		/// - 3-byte block addresses
		/// - terminated with 0xff 0xff 0xff
		/// Since we don't actually need to worry about bad blocks,
		/// just set this entire region to 0xff.
		u8 spared_bad_block_list[505] = { 0xff };

		applepp_storage_device_info(const char *name,
									uint32_t blocks)
		{
			memcpy(device_name, name, 13);
			blocks_available[0] = (u8)((blocks & 0x00FF0000) >> 16);
			blocks_available[1] = (u8)((blocks & 0x0000FF00) >> 8);
			blocks_available[2] = (u8)((blocks & 0x000000FF) >> 8);
		}
	} __attribute__((packed)) device_info;

	applepp_spare_table(const char *name = applepp_device_name_ProFile,
						u32 block_count = applepp_block_count_5MB)
		: device_info(name, block_count)
	{}
};

/// The block address of the RAM buffer used by the device, which can be
/// accessed for diagnostic purposes.
const uint32_t applepp_ram_buffer_address = 0x00fffffe;

/// The block address of the spare table, which is really more of a
/// device info table that also happens to contain the table of spare
/// blocks. This is represented by the applepp_spare_table above.
const uint32_t applepp_spare_table_address = 0x00ffffff;


void applepp_storage_device::device_start()
{
	save_item(NAME(m_strb));
	save_item(NAME(m_rw));
	save_item(NAME(m_cmd));
	save_item(NAME(m_res));
	save_item(NAME(m_pd));

	save_item(NAME(m_current_command));
	save_item(NAME(m_block_address));
	save_item(NAME(m_block_address_pos));
	save_item(NAME(m_block));
	save_item(NAME(m_block_pos));
	save_item(NAME(m_current_status));
	save_item(NAME(m_state));

	m_strb = m_rw = m_cmd = m_res = 1;
	m_pd = 0;
}

void applepp_storage_device::device_reset()
{
	indicate_presence(); // TODO: Use to whether image is attached
	deassert_pbsy();
	reset_state_machine();
}

void applepp_storage_device::pd_w(u8 data)
{
	LOGDATA("pd_w 0x%02x->0x%02x (%s)\n", m_pd, data, machine().describe_context());
	m_pd = data;

	run_state_machine();
}

void applepp_storage_device::pstrb_w(int level)
{
	LOGCTRL("pstrb_w %d->%d (%s)\n", m_strb, level, machine().describe_context());
	if(m_strb == level) return;
	m_strb = level;

	run_state_machine();
}

void applepp_storage_device::prw_w(int level)
{
	LOGCTRL("prw_w %d->%d (%s)\n", m_rw, level, machine().describe_context());
	if(m_rw == level) return; // only trigger state machine on transitions
	m_rw = level;

	run_state_machine();
}

void applepp_storage_device::pcmd_w(int level)
{
	LOGCTRL("pcmd_w %d->%d (%s)\n", m_cmd, level, machine().describe_context());
	if(m_cmd == level) return; // only trigger state machine on transitions
	m_cmd = level;

	run_state_machine();
}

void applepp_storage_device::pres_w(int level)
{
	LOGCTRL("pres_w %d->%d (%s)\n", m_res, level, machine().describe_context());
	if(m_res == level) return; // only trigger state machine on transitions
	m_res = level;

	run_state_machine();
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

void applepp_storage_device::read_block_from_image()
{
	const u32 address = (  (m_block_address[0] << 16)
						 | (m_block_address[1] << 8)
						 | (m_block_address[2]));

	// TODO: Derive name from class.
	const char *name = applepp_device_name_ProFile;

	// TODO: Get size from attached image.
	u32 size = applepp_block_count_5MB;

	if (address < size) {
		// Status comes first for read command.
		memcpy(&m_block[0], m_current_status, 4);

		// TODO: Read the block from the attached image.
	} else if (address == applepp_spare_table_address) {
		// Status comes first for read command.
		memcpy(&m_block[0], m_current_status, 4);

		// Get the spare table into the data area.
		applepp_spare_table info(name, size);
		memcpy(&m_block[4], info.bytes, 532);
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

		// Status comes last for write.
		memcpy(&m_block[532], m_current_status, 4);
	} else if (address == applepp_ram_buffer_address) {
		// The HLE doen't have a RAM buffer to read/write.
		// So do nothing.
	} else {
		// TODO: Generate an error, block out of range.
	}
}

void applepp_storage_device::indicate_presence()
{
	LOGCTRL("pchk_set 0 (%s)\n", machine().describe_context());
	m_connector->pchk_set(0);
}

void applepp_storage_device::assert_pbsy()
{
	LOGCTRL("pbsy_set 0 (%s)" "\n", machine().describe_context());
	m_connector->pbsy_set(0);
}

void applepp_storage_device::deassert_pbsy()
{
	LOGCTRL("pbsy_set 1 (%s)" "\n", machine().describe_context());
	m_connector->pbsy_set(1);
}

void applepp_storage_device::write_data_to_host(u8 data)
{
	LOGDATA("write to host 0x%02x (%s)" "\n", data, machine().describe_context());
	m_connector->pd_set_from_device(data);
}

void applepp_storage_device::reset_state_machine()
{
	LOGSTATE("reset state machine (%s)" "\n", machine().describe_context());

	// Restart the state machine and clear buffers.

	m_current_command = read_command;
	memset(m_block_address, 0, 3);
	memset(m_block, 0, 532);
	m_block_pos = 0;
	memset(m_current_status, 0, 4);
	memset(m_command_buf, 0, 6);
	m_command_buf_idx = 0;
	m_state = IDLE;

	LOGSTATE("IDLE (%s)" "\n", machine().describe_context());
}

void applepp_storage_device::run_state_machine()
{
	switch (m_state) {
		case IDLE:
			if (!m_res) { // negative logic
				reset_state_machine();
			} else if (!m_cmd) { // negative logic
				write_data_to_host(get_a_command);
				assert_pbsy();
				LOGSTATE("IDLE->READY_FOR_CMD (%s)" "\n", machine().describe_context());
				m_state = READY_FOR_CMD;
			}
			break;

		case READY_FOR_CMD:
			if (!m_res) { // negative logic
				reset_state_machine();
				deassert_pbsy();
			} else if (!m_strb && !m_rw) { // negative logic
				if (m_pd == 0x55) {
					deassert_pbsy();
					memset(m_command_buf, 0, 3);
					m_command_buf_idx = 0;
					memset(m_block_address, 0, 3);
					m_state = GETTING_CMD;
					LOGSTATE("READY_FOR_CMD->GETTING_CMD (%s)" "\n", machine().describe_context());
				} else if (m_pd == 0x00) {
					reset_state_machine();
					deassert_pbsy();
				}
			} else {
				// wait for host ack
				// TODO: Timeout?
			}
			break;

		case GETTING_CMD:
			if (!m_res) { // negative logic
				reset_state_machine();
			} else if (m_cmd) { // negative logic, dispatch on cmd deassert
				m_current_command = (applepp_command_t) m_command_buf[0];
				switch (m_current_command) {
					case read_command:
						// allow shorter block addresses
						switch (m_command_buf_idx) {
							case 6:
							case 5:
							case 4:
								m_block_address[0] = m_command_buf[1];
								m_block_address[1] = m_command_buf[2];
								m_block_address[2] = m_command_buf[3];
								break;

							case 3:
								m_block_address[0] = 0x00;
								m_block_address[1] = m_command_buf[1];
								m_block_address[2] = m_command_buf[2];
								break;

							case 2:
								m_block_address[0] = 0x00;
								m_block_address[1] = 0x00;
								m_block_address[2] = m_command_buf[1];
								break;

							case 1:
							case 0:
							default:
								m_block_address[0] = 0x00;
								m_block_address[1] = 0x00;
								m_block_address[2] = 0x00;
								break;
						}
						// ignore sparing and retry counts

						// Acknowledge command
						LOGSTATE("GETTING_CMD->START_READ_CMD (%s)" "\n", machine().describe_context());
						m_state = START_READ_CMD;
						write_data_to_host(read_a_block);
						assert_pbsy();
						break;

					case write_command:
					case write_verify_command:
						LOGSTATE("GETTING_CMD: 0x%02x (write%s) (%s)" "\n", (int)m_current_command, m_current_command == write_verify_command ? "/verify" : "", machine().describe_context());
//                      assert(m_command_buf_idx >= 4);
						m_block_address[0] = m_command_buf[1];
						m_block_address[1] = m_command_buf[2];
						m_block_address[2] = m_command_buf[3];

						// Acknowledge command
						write_data_to_host(((u8)m_current_command) + 0x02);
						assert_pbsy();
						LOGSTATE("GETTING_CMD->START_WRITE_CMD (%s)" "\n", machine().describe_context());
						m_state = START_WRITE_CMD;
						break;

					default:
						// Indicate error.
						LOGSTATE("GETTING_CMD: 0x%02x (%s)\n", (int)m_current_command, machine().describe_context());
						write_data_to_host(0x55);
						assert_pbsy();
						deassert_pbsy();
						LOGSTATE("GETTING_CMD->IDLE (error) (%s)" "\n", machine().describe_context());
						m_state = IDLE;
						break;
				}
			} else if (!m_cmd && !m_strb && !m_rw) { // negative logic, fill command buffer
				LOGSTATE("GETTING_CMD[%d] = 0x%02x (%s)" "\n", m_command_buf_idx, m_pd, machine().describe_context());
				m_command_buf[m_command_buf_idx++] = m_pd;
				assert(m_command_buf_idx <= 6);
			} else {
				// TODO: Timeout?
				LOGSTATE("GETTING_CMD: Unknown transition (%s)" "\n", machine().describe_context());
			}
			break;

		case START_READ_CMD:
			if (!m_res) { // negative logic
				reset_state_machine();
				deassert_pbsy();
			} else if (m_pd == 0x55) {
				read_block_from_image();
				m_block_pos = 0;
				deassert_pbsy();
				LOGSTATE("START_READ_CMD->PERFORM_READ_CMD (%s)" "\n", machine().describe_context());
				m_state = PERFORM_READ_CMD;
			} else {
				// wait for host ack
				// TODO: Timeout?
			}
			break;

		case PERFORM_READ_CMD:
			if (!m_res) { // negative logic
				reset_state_machine();
			} else if (!m_strb) { // negative logic
				// Send the status and read data one byte at a time.
				write_data_to_host(m_block[m_block_pos++]);
				LOGSTATE("PERFORM_READ_CMD->PERFORM_READ_CMD (%s)" "\n", machine().describe_context());
			} else if (!m_cmd) { // negative logic
				// CMD being asserted ends the read
				// that means we're ready for another command
				write_data_to_host(get_a_command);
				assert_pbsy();
				LOGSTATE("PERFORM_READ_CMD->READY_FOR_CMD (%s)" "\n", machine().describe_context());
				m_state = READY_FOR_CMD;
			} else {
				// TODO: Timeout?
			}
			break;

		case START_WRITE_CMD:
			if (!m_res) { // negative logic
				reset_state_machine();
				deassert_pbsy();
			} else if (m_pd == 0x55) {
				m_block_pos = 0;
				deassert_pbsy();
				LOGSTATE("START_WRITE_CMD->PERFORM_WRITE_CMD (%s)" "\n", machine().describe_context());
				m_state = PERFORM_WRITE_CMD;
			} else {
				// wait for host ack
				// TODO: Timeout?
			}
			break;

		case PERFORM_WRITE_CMD:
			if (!m_res) { // negative logic
				reset_state_machine();
			} else if (!m_strb) { // negative logic
				// Receive the written data
				m_block[m_block_pos++] = m_pd;
				LOGSTATE("PERFORM_WRITE_CMD->PERFORM_WRITE_CMD (%s)" "\n", machine().describe_context());
			} else if (!m_cmd) { // negative logic
				// CMD being asserted ends the write, goes to ack phase
				write_block_to_image();
				write_data_to_host(write_actual_data);
				assert_pbsy();
				LOGSTATE("PERFORM_WRITE_CMD->ACK_WRITE_CMD (%s)" "\n", machine().describe_context());
				m_state = ACK_WRITE_CMD;
			}
			break;

		case ACK_WRITE_CMD:
			if (!m_res) { // negative logic
				reset_state_machine();
				deassert_pbsy();
			} else if (m_pd == 0x55) {
				deassert_pbsy();
				LOGSTATE("ACK_WRITE_CMD->READ_WRITE_STATUS (%s)" "\n", machine().describe_context());
				m_state = READ_WRITE_STATUS;
			} else {
				// wait for host ack
				// TODO: Timeout?
			}
			break;

		case READ_WRITE_STATUS:
			if (!m_res) { // negative logic
				reset_state_machine();
			} else if (!m_strb) { // negative logic
				// Send the status data filled in by write to image
				write_data_to_host(m_block[m_block_pos++]);
				LOGSTATE("READ_WRITE_STATUS->READ_WRITE_STATUS (%s)" "\n", machine().describe_context());
			} else if (!m_cmd) { // negative logic
				// CMD being asserted ends the read
				// that means we're ready for another command
				write_data_to_host(get_a_command);
				assert_pbsy();
				LOGSTATE("READ_WRITE_STATUS->READY_FOR_CMD (%s)" "\n", machine().describe_context());
				m_state = READY_FOR_CMD;
			} else {
				// TODO: Timeout?
			}
			break;
	}
}
