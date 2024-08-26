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

#include "socket_util.h"
#include <arpa/inet.h>
#include <bits/types/struct_iovec.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
	// create a socket and save the file descriptor
	int socket_fd = createTCPIPv4Socket();

	// default localhost for now will fix to public ip later
	struct sockaddr_in address = createIPv4Address("0.0.0.0", 8675);
	int result =
		connect(socket_fd, (struct sockaddr *)&address, sizeof(address));

	if (result == 0) {
		printf("Connection was sucessful.\n");
	}

	char *line = NULL;
	size_t line_size = 0;

	while (true) {
		ssize_t char_count = getline(&line, &line_size, stdin);

		if (char_count > 0) {

			if (strcmp(line, "quit\n") == 0) {
				break;
			}
			ssize_t amount_sent = send(socket_fd, line, char_count, 0);
		}
	}

	close(socket_fd);

	return 0;
}
