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
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>

int main() {
	// create and bind server socket
	int server_socket_fd = createTCPIPv4Socket();
	struct sockaddr_in server_address = createIPv4Address("", 8675);
	int bind_result = bind(server_socket_fd, (struct sockaddr *)&server_address,
						   sizeof(server_address));

	if (bind_result == 0) {
		printf("Server socket bound successfully.\n");
	}
	int listen_result = listen(server_socket_fd, 10);

	struct sockaddr_in client_address;
	unsigned int client_address_size = sizeof(client_address);
	int client_socket_fd =
		accept(server_socket_fd, (struct sockaddr *)&client_address,
			   &client_address_size);

	char buffer[1024];
	recv(client_socket_fd, buffer, 1024, 0);

	printf("Client Message: %s\n", buffer);

	return 0;
}