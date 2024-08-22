/*
 * Copyright (c) 2024, Lance Borden
 * All rights reserved.
 *
 * This software is licensed under the BSD 3-Clause License.
 * You may obtain a copy of the license at:
 * https://opensource.org/licenses/BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted under the conditions stated in the BSD 3-Clause
 * License.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "socket_util.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>

int createTCPIPv4Socket() { return socket(AF_INET, SOCK_STREAM, 0); }

struct sockaddr_in *createIPv4Address(char *ip, unsigned int port) {
  struct sockaddr_in *address = malloc(sizeof(struct sockaddr_in));
  address->sin_family = AF_INET;
  address->sin_port = htons(port); // put into big endian for TCP
  inet_pton(AF_INET, ip, &address->sin_addr.s_addr);
  return address;
}
