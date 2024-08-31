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
#include "users_db.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
	int fd;
	struct sockaddr_in addr;
	int err;
	bool success;
} accepted_socket_t;

static accepted_socket_t accept_connection(int fd) {
	struct sockaddr_in client_address;
	unsigned int client_address_size = sizeof(client_address);
	int client_socket_fd =
		accept(fd, (struct sockaddr *)&client_address, &client_address_size);

	accepted_socket_t socket = {0};
	socket.addr = client_address;
	socket.fd = client_socket_fd;
	socket.success = client_socket_fd > 0;

	if (!socket.success) {
		socket.err = client_socket_fd;
	}

	return socket;
}

static void *connection_thread_main(void *arg) {
	int fd = *(int *)arg;
	char buffer[1024];
	while (true) {
		packet_t recv_packet;
		int rc = portal_recv_packet(fd, &recv_packet);

		if (rc == PORTAL_FAIL) {
			break;
		}

		// TODO: make this a switch statement for all packet types
		if (strcmp(recv_packet.header.type, "MSG") == 0) {
			portal_handle_msg(&recv_packet);
		}

		free(recv_packet.data);
	}

	close(fd);

	return NULL;
}

static void open_server_connections(int fd) {
	while (true) {
		accepted_socket_t client_socket = accept_connection(fd);
		pthread_t id;
		pthread_create(&id, NULL, connection_thread_main, &client_socket.fd);
	}
}

int main() {
	// create and bind server socket
	int server_socket_fd = createTCPIPv4Socket();
	struct sockaddr_in server_address = createIPv4Address("", 8675);
	int bind_result = bind(server_socket_fd, (struct sockaddr *)&server_address,
						   sizeof(server_address));

	char ip_str[INET_ADDRSTRLEN];
	if (inet_ntop(AF_INET, (void *)&(server_address.sin_addr), ip_str,
				  sizeof(ip_str)) == NULL) {
		printf("Error in inet_ntop");
		return 1;
	}

	if (bind_result == 0) {
		printf("Server socket bound successfully at: %s:8675\n", ip_str);
	}
	int listen_result = listen(server_socket_fd, 10);

	open_database_thread();
	open_server_connections(server_socket_fd);

	shutdown(server_socket_fd, SHUT_RDWR);
	return 0;
}
