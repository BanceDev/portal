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
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

int createTCPIPv4Socket() { return socket(AF_INET, SOCK_STREAM, 0); }

struct sockaddr_in createIPv4Address(char *ip, unsigned int port) {
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port); // put into big endian for TCP
	if (strlen(ip) == 0) {
		address.sin_addr.s_addr = INADDR_ANY;
	} else {
		inet_pton(AF_INET, ip, &address.sin_addr.s_addr);
	}
	return address;
}

static void serialize_packet(packet_t *packet, unsigned char **buffer,
							 size_t *buffer_size) {
	*buffer_size = sizeof(packet_header_t) + sizeof(size_t) + packet->data_size;

	*buffer = malloc(*buffer_size);
	if (*buffer == NULL) {
		perror("Failed to allocate buffer");
		return;
	}

	// pointer for tracking position in buffer
	unsigned char *ptr = *buffer;

	// serialize header
	int network_id = htonl(packet->header.id);
	memcpy(ptr, &network_id, sizeof(int));
	ptr += sizeof(int);

	memcpy(ptr, packet->header.type, sizeof(packet->header.type));
	ptr += sizeof(packet->header.type);

	// serialize data size
	size_t network_data_size = htonl(packet->data_size);
	memcpy(ptr, &network_data_size, sizeof(size_t));
	ptr += sizeof(size_t);

	// serialize data if exists
	if (packet->data_size > 0 && packet->data != NULL) {
		memcpy(ptr, packet->data, packet->data_size);
	}
}

static void deserialize_packet(unsigned char *buffer, packet_t *packet) {
	unsigned char *ptr = buffer;

	// deserialize header
	int network_id;
	memcpy(&network_id, ptr, sizeof(int));
	packet->header.id = ntohl(network_id);
	ptr += sizeof(int);

	memcpy(packet->header.type, ptr, sizeof(packet->header.type));
	ptr += sizeof(packet->header.type);

	// deserialize data_size
	size_t network_data_size;
	memcpy(&network_data_size, ptr, sizeof(size_t));
	packet->data_size = ntohl(network_data_size);
	ptr += sizeof(size_t);

	// deserialize the actual data
	if (packet->data_size > 0) {
		packet->data = malloc(packet->data_size);
		if (packet->data == NULL) {
			perror("Failed to allocate memory for data");
			return;
		}
		memcpy(packet->data, ptr, packet->data_size);
	} else {
		packet->data = NULL;
	}
}

int portal_send_packet(int fd, packet_t *packet) {
	unsigned char *buffer;
	size_t buffer_size;

	serialize_packet(packet, &buffer, &buffer_size);
	ssize_t sent_bytes = send(fd, buffer, buffer_size, 0);

	if (sent_bytes == 0) {
		perror("Failed to send packet");
		free(buffer);
		return PORTAL_FAIL;
	}

	free(buffer);
	return PORTAL_OK;
}

int portal_recv_packet(int fd, packet_t *packet) {
	// max buffer size is 1024
	unsigned char buffer[1024];

	ssize_t recv_bytes = recv(fd, buffer, sizeof(buffer), 0);
	if (recv_bytes <= 0) {
		perror("Failed to recieve packet");
		return PORTAL_FAIL;
	}

	deserialize_packet(buffer, packet);
	return PORTAL_OK;
}

void portal_handle_msg(packet_t *packet) {
	char buffer[packet->data_size];
	strncpy(buffer, (char *)packet->data, packet->data_size);
	// add null terminator
	buffer[packet->data_size] = 0;

	printf("Client Message: %s\n", buffer);
}
