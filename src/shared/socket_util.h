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

#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <unistd.h>

#define PORTAL_OK 0
#define PORTAL_FAIL 1

typedef struct {
    int id;
    char type[4];
} packet_header_t;

typedef struct {
    packet_header_t header;
    size_t data_size;
    void *data;
} packet_t;

int createTCPIPv4Socket();
struct sockaddr_in createIPv4Address(char *ip, unsigned int port);

int portal_send_packet(int fd, packet_t *packet);
int portal_recv_packet(int fd, packet_t *packet);

// -- packet type handlers --
void portal_handle_msg(packet_t *packet);
#endif // SOCKET_UTIL_H
