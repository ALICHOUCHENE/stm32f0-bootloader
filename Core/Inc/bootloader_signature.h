/**
  ******************************************************************************
  * @file    bootloader_signature.h
  * @brief   Bootloader Digital Signature Algorithm (DSA) parameters.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 ALI CHOUCHENE.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
 ******************************************************************************
 */

#ifdef SECURITY_ENABLED
#include "crypto_dsa.h"

#define SIGNED_FILE_MAGIC_NUMBER	0x424F4F54	//Magic number used to identify a signed file footer ("BOOT").


// Define binary provider public key (Change here the private key!)
static crypto_dsa_public_key_t public_key = {0x96, 0xf3, 0xfb, 0x5f, 0xdc, 0xee, 0x9c, 0x43, 0xf9, 0xe6, 0x15, 0x00, 0x42, 0x8f, 0x6d, 0xdb,
											 0x6c, 0x16, 0x3c, 0xe6, 0x34, 0xc1, 0x26, 0xf4, 0xab, 0x32, 0x27, 0xfe, 0x0c, 0xae, 0x28, 0x08};
/**
 * @struct signed_file_footer_t
 * @brief Footer appended to signed binary files.
 *
 * This structure is appended at the end of a signed binary file.
 * It contains metadata and the cryptographic signature.
 */
typedef struct {
	uint32_t magic;						// Magic number to identify the footer
	uint32_t file_length;				// Original file length (without footer)
	crypto_dsa_signature_t signature;	// Digital signature of the file
} __attribute__((packed)) signed_file_footer_t;

typedef enum {
	authentication_status_success,				// Authentication success
	authentication_status_footer_not_found,		// Footer not found! binary unsigned
	authentication_status_file_length_mismatch,	// File length mismatch
	authentication_status_invalid_signature,	// Invalid Signature
	authentication_status_count,
} authentication_status_t;
#endif