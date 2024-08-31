/*
Copyright (c) 2024, Lance Borden
All rights reserved.

This software is licensed under the BSD 3-Clause License.
You may obtain a copy of the license at:
https://opensource.org/licenses/BSD-3-Clause

Redistribution and use in source and binary forms, with or without
modification, are permitted under the conditions stated in the BSD 3-Clause
License.

THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "crypto.h"
#include "socket_util.h"
#include <argon2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void crypto_generate_salt(unsigned char *salt, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		salt[i] = rand() % 256;
	}
}

int crypto_generate_hash_with_salt(user_t *usr, const char *psswd) {
	srand(time(NULL));

	unsigned char salt[16];
	crypto_generate_salt(salt, sizeof(salt));

	// Argon2 Params
	char encoded_hash[128];
	uint32_t t_cost = 2;
	uint32_t m_cost = 1 << 17; // 128 MB
	uint32_t parallelism = 12;

	int rc = argon2i_hash_raw(t_cost, m_cost, parallelism, psswd, strlen(psswd),
							  salt, strlen(salt), encoded_hash,
							  sizeof(encoded_hash));

	if (rc != ARGON2_OK) {
		printf("Error: %s\n", argon2_error_message(rc));
		return PORTAL_FAIL;
	}

	// copy into the user struct
	memcpy(usr->psswd_hash, encoded_hash, sizeof(encoded_hash));
	memcpy(usr->psswd_salt, salt, sizeof(salt));

	return PORTAL_OK;
}
