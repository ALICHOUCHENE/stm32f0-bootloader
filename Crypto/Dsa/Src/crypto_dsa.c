/**
 ******************************************************************************
 * @file    crypto_dsa.c
 * @brief   This file contains the implementation of DSA.
 *
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

#include <crypto_dsa.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "monocypher.h"

/*
 * **********************************************************************************
 * External APIs
 * **********************************************************************************
 */
 
/**
 * @brief Verify a digital signature.
 *
 * Checks whether the provided signature is valid for
 * the given message and public key.
 *
 * @param[in] public_key     Public key used for verification.
 * @param[in] data           Pointer to the message buffer.
 * @param[in] message_size   Size of the message in bytes.
 * @param[in] signature      Signature to verify.
 *
 * @return true  If the signature is valid.
 * @return false If the signature is invalid.
 */
bool crypto_dsa_verify(crypto_dsa_public_key_t public_key,
                       const uint8_t *data,
                       size_t message_size,
                       crypto_dsa_signature_t signature)
{
	// Sanity check
	if (!data || !public_key || !signature) {
		return false;
	}
	
	// Verify signature
	return crypto_eddsa_check(signature, public_key, data, message_size) == 0 ? true : false;
}
