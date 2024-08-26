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

#ifndef WARP_H
#define WARP_H

#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

// -- colors --
#define WP_PRIMARY_ITEM_COLOR                                                  \
	(wp_color) { 133, 138, 148, 255 }
#define WP_SECONDARY_ITEM_COLOR                                                \
	(wp_color) { 96, 100, 107, 255 }

#define WP_NO_COLOR                                                            \
	(wp_color) { 0, 0, 0, 0 }
#define WP_WHITE                                                               \
	(wp_color) { 255, 255, 255, 255 }
#define WP_BLACK                                                               \
	(wp_color) { 0, 0, 0, 255 }
#define WP_RED                                                                 \
	(wp_color) { 255, 0, 0, 255 }
#define WP_GREEN                                                               \
	(wp_color) { 0, 255, 0, 255 }
#define WP_BLUE                                                                \
	(wp_color) { 0, 0, 255, 255 }

typedef struct {
	uint8_t r, g, b, a;
} wp_color;

// -- events --

typedef struct {
	int32_t keycode;
	bool happened, pressed;
} wp_key_event;

typedef struct {
	int32_t button_code;
	bool happened, pressed;
} wp_mouse_button_event;

typedef struct {
	int32_t x, y;
	bool happened;
} wp_cursor_pos_event;

typedef struct {
	int32_t x_off, y_off;
	bool happened;
} wp_scroll_event;

typedef struct {
	int32_t charcode;
	bool happened;
} wp_char_event;

// -- ui elements --

typedef struct {
	uint32_t id;
	uint32_t width, height;
} wp_texture;

typedef struct {
	void *cdata;
	void *font_info;
	uint32_t tex_width, tex_height;
	uint32_t line_gap_add, font_size;
	wp_texture texture;
	uint32_t num_glyphs;
} wp_font;

typedef enum { WP_LINEAR = 0, WP_NEAREST } wp_texture_filtering;

typedef struct {
	float width, height;
	int32_t end_x, end_y;
	uint32_t rendered_count;
} wp_text_props;

typedef struct {
	int32_t cursor_index, width, height, start_height;
	char *buf;
	uint32_t buf_size;
	char *placeholder;
	bool selected;

	uint32_t max_chars;

	int32_t selection_start, selection_end, mouse_selection_start,
		mouse_selection_end;
	int32_t selection_dir, mouse_dir;

	bool _init;

	void (*char_callback)(char);
	void (*insert_override_callback)(void *);
	void (*key_callback)(void *);

	bool retain_height;
} wp_input_field;

typedef struct {
	void *val;
	int32_t handle_pos;
	bool _init;
	float min, max;
	bool held, selcted;
	float width;
	float height;
	uint32_t handle_size;
	wp_color handle_color;
} wp_slider;

typedef enum {
	WP_RELEASED = -1,
	WP_IDLE = 0,
	WP_HOVERED = 1,
	WP_CLICKED = 2,
	WP_HELD = 3,
} wp_clickable_state;

typedef struct {
	wp_color color, hover_color;
	wp_color text_color, hover_text_color;
	wp_color border_color;
	float padding;
	float margin_left;
	float margin_right;
	float margin_top;
	float margin_bottom;
	float border_width;
	float corner_radius;
} wp_element_props;

typedef struct {
	vec2s pos, size;
} wp_aabb; // axis aligned bounding box

typedef struct {
	wp_element_props button_props, div_props, text_props, image_props,
		inputfield_props, checkbox_props, slider_props, scrollbar_props;
	wp_font font;
	bool div_smooth_scroll;
	float div_scroll_acceleration, div_scroll_max_velocity;
	float div_scroll_amount_px;
	float div_scroll_velocity_deceleration;

	float scrollbar_width;
} wp_theme;

typedef struct {
	int64_t id;
	wp_aabb aabb;
	wp_clickable_state interact_state;

	bool scrollable;

	vec2s total_area;
} wp_div;
#endif // WARP_H