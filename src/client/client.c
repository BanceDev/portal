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
#include "warp/warp.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <arpa/inet.h>
#include <bits/types/struct_iovec.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MESSAGE_BUF_SIZE 1024
#define WIN_INIT_W 1280
#define WIN_INIT_H 720
#define GLOBAL_MARGIN 25.0f

// struct to handle global app state
typedef struct {
	GLFWwindow *win;
	int32_t winw, winh;

	wp_input_field message_input;
	char message_buffer[MESSAGE_BUF_SIZE];
} state;

// create global singleton of state and socket
static state s;
static int socket_fd;

static void resizecb(GLFWwindow *win, int32_t w, int32_t h) {
	s.winw = w;
	s.winh = h;
	wp_resize_display(w, h);
	glViewport(0, 0, w, h);
}

static void init_sockets() {
	socket_fd = createTCPIPv4Socket();

	// default localhost for now will fix to public ip later
	struct sockaddr_in address = createIPv4Address("0.0.0.0", 8675);
	int result =
		connect(socket_fd, (struct sockaddr *)&address, sizeof(address));

	if (result == 0) {
		printf("Connection was sucessful.\n");
	}
}

static void init_window() {
	glfwInit();

	s.winw = WIN_INIT_W;
	s.winh = WIN_INIT_H;

	s.win = glfwCreateWindow(s.winw, s.winh, "Portal", NULL, NULL);
	glfwMakeContextCurrent(s.win);
	glfwSetFramebufferSizeCallback(s.win, resizecb);
	wp_init_glfw(s.winw, s.winh, s.win);
}

static void init_ui() {
	memset(s.message_buffer, 0, MESSAGE_BUF_SIZE);
	s.message_input = (wp_input_field){.width = 400,
									   .buf = s.message_buffer,
									   .buf_size = MESSAGE_BUF_SIZE,
									   .placeholder = (char *)"message"};
}

static void terminate() {
	wp_terminate();
	close(socket_fd);

	glfwDestroyWindow(s.win);
	glfwTerminate();
}

// TODO: prevent client from sending message if size is over 1024
// makes max message size 1024-16 (for header and size bytes)
static void send_text_message(char *line) {

	// null terminator char is handled server side
	size_t char_count = strlen(line);
	if (char_count > 0) {

		packet_t msg_packet;
		msg_packet.header.id = 0;
		strncpy(msg_packet.header.type, "MSG", 4);
		msg_packet.data_size = char_count;
		msg_packet.data = malloc(char_count);
		memcpy(msg_packet.data, line, char_count);

		portal_send_packet(socket_fd, &msg_packet);

		free(msg_packet.data);
	}

	s.message_input.buf[0] = '\0';
}

static void render_main_screen() {
	// message input field

	{
		wp_text("Send a message:");

		wp_next_line();
		wp_element_props props = wp_get_theme().inputfield_props;
		props.padding = 15;
		props.border_width = 0;
		props.color = WP_BLACK;
		props.corner_radius = 11;
		props.text_color = WP_WHITE;
		props.border_width = 1.0f;
		props.border_color = WP_WHITE;
		props.corner_radius = 2.5f;
		props.margin_bottom = 10.0f;
		wp_push_style_props(props);
		wp_input_text(&s.message_input);
		wp_pop_style_props();
	}

	// send button
	{
		const float width = 150.0f;

		wp_element_props props = wp_get_theme().button_props;
		props.border_width = 0.0f;
		props.margin_top = 0.0f;
		props.corner_radius = 4.0f;
		wp_push_style_props(props);

		wp_set_line_should_overflow(false);
		if (wp_button_fixed("send", width, -1) == WP_CLICKED) {
			send_text_message(s.message_input.buf);
		}
		wp_set_line_should_overflow(true);
		wp_pop_style_props();
	}
}

int main() {
	init_window();
	init_ui();
	init_sockets();

	while (!glfwWindowShouldClose(s.win)) {
		glClear(GL_COLOR_BUFFER_BIT);

		wp_begin();

		wp_div_begin(((vec2s){GLOBAL_MARGIN, GLOBAL_MARGIN}),
					 ((vec2s){s.winw - GLOBAL_MARGIN * 2.0f,
							  s.winh - GLOBAL_MARGIN * 2.0f}),
					 true);

		render_main_screen();

		wp_div_end();
		wp_end();

		glfwPollEvents();
		glfwSwapBuffers(s.win);
	}
	terminate();
	return 0;
}
