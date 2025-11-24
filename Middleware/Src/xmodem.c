/**
  ******************************************************************************
  * @file    xmodem.c
  * @brief   This file contains xmodem transfer protocol implementation
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 IZITRON.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
 ******************************************************************************
  */

#include <stdbool.h>
#include "stm32f0xx_hal.h"
#include "serial.h"
#include "xmodem.h"
#include "string.h"
#include "system.h"

// XMODEM protocol control characters
#define XMODEM_SOH    0x01  // Start of Header: marks the beginning of a 128-byte data packet
#define XMODEM_DLE    0x10  // Data Link Escape: mainly used for escaping control characters
#define XMODEM_EOT    0x04  // End of Transmission: indicates the sender has finished sending the file
#define XMODEM_ACK    0x06  // Acknowledge: sent by receiver to confirm a successfully received packet
#define XMODEM_NACK   0x15  // Not Acknowledge: sent by receiver to request retransmission of a corrupted packet
#define XMODEM_CAN    0x18  // Cancel: used to abort the transfer; typically requires two consecutive CAN bytes

#define XMODEM_DELAY_ACK_NACK			20	 		// 20 ms delay between
#define XMODEM_INIT_DELAY				1000		// 1 second delay at start of transfer
#define XMODEM_INIT_MAX_DELAY			30000		// 30 seconds to wait for the sender to respond in initialization phase
#define XMODEM_PACKET_WAIT_MAX_DELAY	1000		// 1 seconds to wait for the sender to send a packet after a ACK or NACK

/*
XMODEM Packet Structure (one frame):

+-----+-----+------------+--------------------------------+-------+
| SOH | PKT | 255-PKT    |             DATA               | CRC   |
| 1B  | 1B  | 1B         | 128B (or less, padded last)    | 2B    |
+-----+-----+------------+--------------------------------+-------+

- SOH       : Start of Header (0x01)
- PKT       : Packet number (1..255)
- 255-PKT   : 1-byte complement of the packet number
- DATA      : Actual file data (128 bytes per packet; last packet may be padded with 0x1A)
- CRC       : 16-bit CRC16 checksum of the DATA section
*/
#define XMODEM_FRAME_LENGTH		XMODEM_HEADER_LENGTH + XMODEM_DATA_CHUNK_SIZE + XMODEM_CRC_LENGTH

// XMODEM transfer states
typedef enum {
    xmodem_state_idle,          // No transfer is currently active
    xmodem_state_initializing,  // Transfer is starting, receiver sending initial NAK/CRC request
    xmodem_state_waiting,       // Waiting for the next packet from the sender
    xmodem_state_receiving,     // Currently receiving a packet
    xmodem_state_aborting,      // Transfer encountered an error, preparing to abort
    xmodem_state_aborted,       // Transfer was aborted (either by receiver sending CAN or sender canceled)
    xmodem_state_completed,     // Transfer completed successfully; EOT received and acknowledged
    xmodem_state_count,         // Total number of states (useful for validation or array sizing)
} xmodem_state_t;

typedef struct {
	const serial_interface_t* serial_interface;  // Pointer to the serial interface used for XMODEM communication
	xmodem_state_t state;                        // Current state of the XMODEM transfer process
	uint8_t buffer[XMODEM_FRAME_LENGTH];         // Buffer to store the current XMODEM frame data
	uint16_t len;                                // Length of valid data in the current frame
	bool packet_correct;                         // Flag indicating whether the last received packet was valid
	xmodem_user_callback_t user_cb;              // User callback function for XMODEM events
} xmodem_ctx_t;
static xmodem_ctx_t _xmodem_ctx;


/******************************************************************************/
/*                          Local Function Prototypes                         */
/*  Description : Declarations of static (file-scope) functions used only     */
/*                within this source file. These functions are not visible    */
/*                outside this module.                                        */
/******************************************************************************/

static void _serial_user_cb(void)
{
	size_t len;

	// Data received over the serial interface
	_xmodem_ctx.serial_interface->read(_xmodem_ctx.buffer + _xmodem_ctx.len, &len);
	_xmodem_ctx.len += len;

	// Check if the first byte is SOH
	if ((_xmodem_ctx.state == xmodem_state_initializing) || (_xmodem_ctx.state == xmodem_state_waiting)) {
		if (_xmodem_ctx.buffer[0] == XMODEM_SOH) {
			// The start of a new frame
			_xmodem_ctx.state = xmodem_state_receiving;
		} else if (_xmodem_ctx.buffer[0] == XMODEM_EOT) {
			// File transfer completed
			_xmodem_ctx.state = xmodem_state_completed;
		} else if ((_xmodem_ctx.len >= 2) && (_xmodem_ctx.buffer[0] == XMODEM_CAN) && (_xmodem_ctx.buffer[1] == XMODEM_CAN)) {
			// Transfer aborted by the sender
			_xmodem_ctx.state = xmodem_state_aborted;
		} else {
			// An error occurred, abort the transfer
			_xmodem_ctx.state = xmodem_state_aborting;
		}
	}
}

static void _xmodem_send_ack(void)
{
	uint8_t ack =  XMODEM_ACK;
	_xmodem_ctx.serial_interface->write(&ack, 1);
}

static void _xmodem_send_nack(void)
{
	uint8_t nack =  XMODEM_NACK;
	_xmodem_ctx.serial_interface->write(&nack, 1);
}

static void _xmodem_send_cancel(void)
{
	uint8_t can_seq[2] = {XMODEM_CAN, XMODEM_CAN};
	_xmodem_ctx.serial_interface->write(can_seq, 2);
}

static void _xmodem_transfer_init(void)
{
	uint8_t _start_seq = 'C';
	uint16_t waiting_time = 0;

	// Send "C" for Xmodem 128 bytes and wait for the SOT
	while (_xmodem_ctx.state == xmodem_state_initializing) {
		_xmodem_ctx.serial_interface->write(&_start_seq, 1);
		HAL_Delay(XMODEM_INIT_DELAY);

		// Check if maximum waiting time is reached
		waiting_time += XMODEM_INIT_DELAY;
		if (waiting_time >= XMODEM_INIT_MAX_DELAY) {
			// Abort the transfer
			_xmodem_ctx.state = xmodem_state_aborting;
		}
	}
}

static uint16_t _xmodem_crc(uint8_t *data, uint32_t length)
{
    uint16_t crc = 0x0000;

    while (length--) {
        crc ^= ((uint16_t)(*data++) << 8);
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc & 0xFFFF;
}

static void _xmodem_rx_process(void)
{
	uint16_t pckt_crc;
	uint16_t crc;
	bool success = false;

	if (_xmodem_ctx.len < XMODEM_FRAME_LENGTH) {
		// Not complete
		return;
	}

	// Check Packet number in packet header
	if (_xmodem_ctx.buffer[1] + _xmodem_ctx.buffer[2] != 255) {
		goto _cleanup;
	}

	// Extract packet CRC
	pckt_crc = (_xmodem_ctx.buffer[XMODEM_FRAME_LENGTH - 2] << 8) | (_xmodem_ctx.buffer[XMODEM_FRAME_LENGTH - 1]);

	// Calculate packet CRC
	crc = _xmodem_crc(&_xmodem_ctx.buffer[XMODEM_HEADER_LENGTH], XMODEM_DATA_CHUNK_SIZE);

	// Check CRC
	if (crc != pckt_crc) {
		goto _cleanup;
	}
#if 1
	// Inform user
	if (_xmodem_ctx.user_cb) {
		_xmodem_ctx.user_cb(xmodem_result_rx_success, &_xmodem_ctx.buffer[XMODEM_HEADER_LENGTH], XMODEM_DATA_CHUNK_SIZE);
	}
#endif
	// Packet is correct
	success = true;

_cleanup:
	memset(_xmodem_ctx.buffer, 0, XMODEM_FRAME_LENGTH);
	_xmodem_ctx.len = 0;
	_xmodem_ctx.packet_correct = success;

	// Wait for the next packet
	if (_xmodem_ctx.state == xmodem_state_receiving) {
		_xmodem_ctx.state = xmodem_state_waiting;
	}
}

static void _xmodem_transfer_wait(void)
{
	uint16_t waiting_time = 0;

	while (_xmodem_ctx.state == xmodem_state_waiting) {
		if (_xmodem_ctx.packet_correct) {
			// Frame received is correct, send ACK
			_xmodem_send_ack();
		} else {
			// Frame corrupted, send NACK
			_xmodem_send_nack();
		}
		// Give the sender some time to process
		HAL_Delay(XMODEM_DELAY_ACK_NACK);

		// Check if maximum waiting time is reached
		waiting_time += XMODEM_DELAY_ACK_NACK;
		if (waiting_time >= XMODEM_PACKET_WAIT_MAX_DELAY) {
			// Abort the transfer
			_xmodem_ctx.state = xmodem_state_aborting;
		}
	}
}

static void _xmodem_completed(void)
{
	// Send a final ACK
	_xmodem_send_ack();

	// Close the serial interface
	_xmodem_ctx.serial_interface->close();

	// Force the state idle
	_xmodem_ctx.state = xmodem_state_idle;

	// Inform the user
	if (_xmodem_ctx.user_cb) {
		_xmodem_ctx.user_cb(xmodem_result_completed, NULL, 0);
	}
}

static void _xmodem_aborting(void)
{
	// Send cancel
	_xmodem_send_cancel();

	// Move to the state aborted
	_xmodem_ctx.state = xmodem_state_aborted;
}

static void _xmodem_aborted(void)
{
	// Close the serial interface
	_xmodem_ctx.serial_interface->close();

	// Force the state idle
	_xmodem_ctx.state = xmodem_state_idle;

	// Inform the user
	if (_xmodem_ctx.user_cb) {
		_xmodem_ctx.user_cb(xmodem_result_aborted, NULL, 0);
	}
}

/******************************************************************************/
/*                              Public API Functions                          */
/*  Description : Definitions of externally visible functions declared in the */
/*                corresponding header file (.h). These functions provide the */
/*                module's interface for use by other components.             */
/******************************************************************************/

void xmodem_stop(void)
{
	if (_xmodem_ctx.state == xmodem_state_idle) {
		return;
	}

	// Abort the transfer
	_xmodem_ctx.state = xmodem_state_aborting;
}

void xmodem_start(xmodem_user_callback_t user_cb)
{

	// Check if Xmodem transfer is ongoing
	if (_xmodem_ctx.state != xmodem_state_idle) {
		// Busy
		return;
	}

	// Clear context
	memset(&_xmodem_ctx, 0, sizeof(xmodem_ctx_t));

	// Register the user callback
	_xmodem_ctx.user_cb = user_cb;

	// Start the serial interface
	_xmodem_ctx.serial_interface = serial_interface_init();
	_xmodem_ctx.serial_interface->open((serial_user_callback_t)_serial_user_cb);
	HAL_Delay(100);

	// Initialize Xmodem state
	_xmodem_ctx.state = xmodem_state_initializing;

	// Start Xmodem transfer
	while(1) {

		switch (_xmodem_ctx.state) {
		case xmodem_state_initializing:
			_xmodem_transfer_init();
			break;

		case xmodem_state_receiving:
			_xmodem_rx_process();
			break;

		case xmodem_state_waiting:
			_xmodem_transfer_wait();
			break;

		case xmodem_state_aborting:
			_xmodem_aborting();
			break;

		case xmodem_state_completed:
			_xmodem_completed();
			// Done
			return;

		case xmodem_state_aborted:
			_xmodem_aborted();
			// Done
			return;

		default:
			break;
		}
	}
}
