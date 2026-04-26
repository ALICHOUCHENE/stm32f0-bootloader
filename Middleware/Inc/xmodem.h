/**
  ******************************************************************************
  * @file    xmodem.h
  * @brief   Header file for XMODEM transfer protocol.
  *          Contains constants, enums, typedefs, and API declarations for
  *          implementing the XMODEM-CRC receiver on STM32.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 ALI CHOUCHENE.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file is provided, this software is delivered "AS-IS".
  *
  ******************************************************************************
  */


#ifndef __XMODEM_H
#define __XMODEM_H

#define XMODEM_DATA_CHUNK_SIZE 	128  // Number of bytes of actual data per packet
#define XMODEM_CRC_LENGTH 		2    // Number of bytes used for CRC16 checksum
#define XMODEM_HEADER_LENGTH 	3    // Number of bytes in the packet header

// XMODEM protocol control characters
#define XMODEM_SOH    0x01  // Start of Header: marks the beginning of a 128-byte data packet
#define XMODEM_DLE    0x10  // Data Link Escape: mainly used for escaping control characters
#define XMODEM_EOT    0x04  // End of Transmission: indicates the sender has finished sending the file
#define XMODEM_ACK    0x06  // Acknowledge: sent by receiver to confirm a successfully received packet
#define XMODEM_NACK   0x15  // Not Acknowledge: sent by receiver to request retransmission of a corrupted packet
#define XMODEM_CAN    0x18  // Cancel: used to abort the transfer; typically requires two consecutive CAN bytes
#define XMODEM_EOF    0x1A  // EOF: End Of File

// XMODEM transfer result codes
typedef enum {
    xmodem_result_rx_success = 0, // A single packet was received successfully and passed CRC check
    xmodem_result_completed,      // The entire file transfer completed successfully (EOT received and acknowledged)
    xmodem_result_aborted,        // Transfer was aborted either by the sender or receiver (CAN detected)
    xmodem_result_count,          // Total number of result codes
} xmodem_result_t;

// User callback function type for XMODEM transfer notifications
typedef void  (*xmodem_user_callback_t)(xmodem_result_t, uint8_t*, uint16_t);

/**
 * @brief Start an XMODEM receive transfer.
 *
 * This function initializes the XMODEM context and begins receiving data
 * from the sender over the serial interface. It blocks internally while
 * handling the XMODEM state machine until the transfer completes, is aborted,
 * or an error occurs.
 *
 * @param user_cb A callback function provided by the user that will be called
 *                for each received packet, when the transfer completes, or
 *                if the transfer is aborted.
 *
 * @note The callback receives:
 *       - xmodem_result_rx_success for a successfully received packet,
 *       - xmodem_result_completed when the full transfer finishes,
 *       - xmodem_result_aborted if the transfer is canceled.
 * @note The data pointer passed to the callback contains the received packet
 *       payload (valid only for xmodem_result_rx_success).
 */
void xmodem_start(xmodem_user_callback_t user_cb);

/**
 * @brief Stop an ongoing XMODEM transfer.
 *
 * This function can be called by the receiver to abort a running transfer.
 * It updates the internal state to indicate that the transfer is aborted
 * and clears buffers. Ensures the serial interface is properly stopped and cleaned up.
 *
 * @note If called while no transfer is active, this function has no effect.
 */
void xmodem_stop(void);


#endif
