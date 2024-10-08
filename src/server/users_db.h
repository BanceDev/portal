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

#ifndef USERS_DB_H
#define USERS_DB_H

#include <stdint.h>

typedef struct {
    uint32_t id;
    const unsigned char* email;
    const unsigned char* username;
    unsigned char psswd_hash[128];
    unsigned char psswd_salt[16] ;
} user_t;

void open_database_thread();

#endif // USERS_DB_H
