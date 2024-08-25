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

#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

int createTCPIPv4Socket();
struct sockaddr_in createIPv4Address(char *ip, unsigned int port);

#endif // SOCKET_UTIL_H