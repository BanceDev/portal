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

typedef void (*wp_menu_item_callback)(uint32_t *);

void wp_init_glfw(uint32_t display_width, uint32_t display_height,
				  void *glfw_window);

void wp_terminate();

wp_theme wp_default_theme();

wp_theme wp_get_theme();

void wp_set_theme(wp_theme theme);

void wp_resize_display(uint32_t display_width, uint32_t display_height);

wp_font wp_load_font(const char *filepath, uint32_t size);

wp_font wp_load_font_ex(const char *filepath, uint32_t size, uint32_t bitmap_w,
						uint32_t bitmap_h);

wp_texture wp_load_texture(const char *filepath, bool flip,
						   wp_texture_filtering filter);

wp_texture wp_load_texture_resized(const char *filepath, bool flip,
								   wp_texture_filtering filter, uint32_t w,
								   uint32_t h);

wp_texture wp_load_texture_resized_factor(const char *filepath, bool flip,
										  wp_texture_filtering filter,
										  float wfactor, float hfactor);

wp_texture wp_load_texture_from_memory(const void *data, size_t size, bool flip,
									   wp_texture_filtering filter);

wp_texture wp_load_texture_from_memory_resized(const void *data, size_t size,
											   bool flip,
											   wp_texture_filtering filter,
											   uint32_t w, uint32_t h);

wp_texture wp_load_texture_from_memory_resized_factor(
	const void *data, size_t size, bool flip, wp_texture_filtering filter,
	float wfactor, float hfactor);

wp_texture wp_load_texture_from_memory_resized_to_fit(
	const void *data, size_t size, bool flip, wp_texture_filtering filter,
	int32_t container_w, int32_t container_h);

unsigned char *wp_load_texture_data(const char *filepath, int32_t *width,
									int32_t *height, int32_t *channels,
									bool flip);

unsigned char *wp_load_texture_data_resized(const char *filepath, int32_t w,
											int32_t h, int32_t *channels,
											bool flip);

unsigned char *wp_load_texture_data_resized_factor(
	const char *filepath, int32_t wfactor, int32_t hfactor, int32_t *width,
	int32_t *height, int32_t *channels, bool flip);

unsigned char *wp_load_texture_data_from_memory(const void *data, size_t size,
												int32_t *width, int32_t *height,
												int32_t *channels, bool flip);

unsigned char *wp_load_texture_data_from_memory_resized(
	const void *data, size_t size, int32_t *channels, int32_t *o_w,
	int32_t *o_h, bool flip, uint32_t w, uint32_t h);

unsigned char *wp_load_texture_data_from_memory_resized_to_fit_ex(
	const void *data, size_t size, int32_t *o_width, int32_t *o_height,
	int32_t i_channels, int32_t i_width, int32_t i_height, bool flip,
	int32_t container_w, int32_t container_h);

unsigned char *wp_load_texture_data_from_memory_resized_to_fit(
	const void *data, size_t size, int32_t *o_width, int32_t *o_height,
	int32_t *o_channels, bool flip, int32_t container_w, int32_t container_h);

unsigned char *wp_load_texture_data_from_memory_resized_factor(
	const void *data, size_t size, int32_t *width, int32_t *height,
	int32_t *channels, bool flip, float wfactor, float hfactor);

void wp_create_texture_from_image_data(unsigned char *data, uint32_t *id,
									   int32_t width, int32_t height,
									   int32_t channels,
									   wp_texture_filtering filter);

void wp_free_texture(wp_texture *tex);

void wp_free_font(wp_font *font);

wp_font wp_load_font_asset(const char *asset_name, const char *file_extension,
						   uint32_t font_size);

wp_texture wp_load_texture_asset(const char *asset_name,
								 const char *file_extension);

void wp_add_key_callback(void *cb);

void wp_add_mouse_button_callback(void *cb);

void wp_add_scroll_callback(void *cb);

void wp_add_cursor_pos_callback(void *cb);

bool wp_key_down(uint32_t key);

bool wp_key_held(uint32_t key);

bool wp_key_up(uint32_t key);

bool wp_key_changed(uint32_t key);

bool wp_mouse_button_down(uint32_t button);

bool wp_mouse_button_held(uint32_t button);

bool wp_mouse_button_up(uint32_t button);

bool wp_mouse_button_changed(uint32_t button);

bool wp_mouse_button_down_on_div(uint32_t button);

bool wp_mouse_button_released_on_div(uint32_t button);

bool wp_mouse_button_changed_on_div(uint32_t button);

double wp_get_mouse_x();

double wp_get_mouse_y();

double wp_get_mouse_x_delta();

double wp_get_mouse_y_delta();

double wp_get_mouse_scroll_x();

double wp_get_mouse_scroll_y();

#define wp_div_begin(pos, size, scrollable)                                    \
	{                                                                          \
		static float scroll = 0.0f;                                            \
		static float scroll_velocity = 0.0f;                                   \
		_wp_div_begin_loc(pos, size, scrollable, &scroll, &scroll_velocity,    \
						  __FILE__, __LINE__);                                 \
	}

#define wp_div_begin_ex(pos, size, scrollable, scroll_ptr,                     \
						scroll_velocity_ptr)                                   \
	_wp_div_begin_loc(pos, size, scrollable, scroll_ptr, scroll_velocity_ptr,  \
					  __FILE__, __LINE__);

wp_div *_wp_div_begin_loc(vec2s pos, vec2s size, bool scrollable, float *scroll,
						  float *scroll_velocity, const char *file,
						  int32_t line);

void wp_div_end();

wp_clickable_state _wp_item_loc(vec2s size, const char *file, int32_t line);
#define wp_item(size) _wp_item_loc(size, __FILE__, __LINE__)

#define wp_button(text) _wp_button_loc(text, __FILE__, __LINE__)
wp_clickable_state _wp_button_loc(const char *text, const char *file,
								  int32_t line);

#define wp_button_wide(text) _wp_button_wide_loc(text, __FILE__, __LINE__)
wp_clickable_state _wp_button_wide_loc(const wchar_t *text, const char *file,
									   int32_t line);

#define wp_image_button(img) _wp_image_button_loc(img, __FILE__, __LINE__)
wp_clickable_state _wp_image_button_loc(wp_texture img, const char *file,
										int32_t line);

#define wp_image_button_fixed(img, width, height)                              \
	_wp_image_button_fixed_loc(img, width, height, __FILE__, __LINE__)
wp_clickable_state _wp_image_button_fixed_loc(wp_texture img, float width,
											  float height, const char *file,
											  int32_t line);

#define wp_button_fixed(text, width, height)                                   \
	_wp_button_fixed_loc(text, width, height, __FILE__, __LINE__)
wp_clickable_state _wp_button_fixed_loc(const char *text, float width,
										float height, const char *file,
										int32_t line);

#define wp_button_fixed_wide(text, width, height)                              \
	_wp_button_fixed_loc_wide(text, width, height, __FILE__, __LINE__)
wp_clickable_state _wp_button_fixed_wide_loc(const wchar_t *text, float width,
											 float height, const char *file,
											 int32_t line);

#define wp_slider_int(slider) _wp_slider_int_loc(slider, __FILE__, __LINE__)
wp_clickable_state _wp_slider_int_loc(wp_slider *slider, const char *file,
									  int32_t line);

#define wp_slider_int_inl_ex(slider_val, slider_min, slider_max, slider_width, \
							 slider_height, slider_handle_size, state)         \
	{                                                                          \
		static wp_slider slider = {.val = slider_val,                          \
								   .handle_pos = 0,                            \
								   .min = slider_min,                          \
								   .max = slider_max,                          \
								   .width = slider_width,                      \
								   .height = slider_height,                    \
								   .handle_size = slider_handle_size};         \
		state = wp_slider_int(&slider);                                        \
	}

#define wp_slider_int_inl(slider_val, slider_min, slider_max, state)           \
	wp_slider_int_inl_ex(slider_val, slider_min, slider_max,                   \
						 wp_get_current_div().aabb.size.x / 2.0f, 5, 0, state)

#define wp_progress_bar_val(width, height, min, max, val)                      \
	_wp_progress_bar_val_loc(width, height, min, max, val, __FILE__, __LINE__)
wp_clickable_state _wp_progress_bar_val_loc(float width, float height,
											int32_t min, int32_t max,
											int32_t val, const char *file,
											int32_t line);

#define wp_progress_bar_int(val, min, max, width, height)                      \
	_wp_progress_bar_int_loc(val, min, max, width, height, __FILE__, __LINE__)
wp_clickable_state _wp_progress_bar_int_loc(float val, float min, float max,
											float width, float height,
											const char *file, int32_t line);

#define wp_progress_stripe_int(slider)                                         \
	_wp_progresss_stripe_int_loc(slider, __FILE__, __LINE__)
wp_clickable_state _wp_progress_stripe_int_loc(wp_slider *slider,
											   const char *file, int32_t line);

#define wp_checkbox(text, val, tick_color, tex_color)                          \
	_wp_checkbox_loc(text, val, tick_color, tex_color, __FILE__, __LINE__)
wp_clickable_state _wp_checkbox_loc(const char *text, bool *val,
									wp_color tick_color, wp_color tex_color,
									const char *file, int32_t line);

#define wp_checkbox_wide(text, val, tick_color, tex_color)                     \
	_wp_checkbox_wide_loc(text, val, tick_color, tex_color, __FILE__, __LINE__)
wp_clickable_state _wp_checkbox_wide_loc(const wchar_t *text, bool *val,
										 wp_color tick_color,
										 wp_color tex_color, const char *file,
										 int32_t line);

#define wp_menu_item_list(items, item_count, selected_index, per_cb, vertical) \
	_wp_menu_item_list_loc(__FILE__, __LINE__, items, item_count,              \
						   selected_index, per_cb, vertical)
int32_t _wp_menu_item_list_loc(const char **items, uint32_t item_count,
							   int32_t selected_index,
							   wp_menu_item_callback per_cb, bool vertical,
							   const char *file, int32_t line);

#define wp_menu_item_list_wide(items, item_count, selected_index, per_cb,      \
							   vertical)                                       \
	_wp_menu_item_list_loc_wide(__FILE__, __LINE__, items, item_count,         \
								selected_index, per_cb, vertical)
int32_t _wp_menu_item_list_loc_wide(const wchar_t **items, uint32_t item_count,
									int32_t selected_index,
									wp_menu_item_callback per_cb, bool vertical,
									const char *file, int32_t line);

#define wp_dropdown_menu(items, placeholder, item_count, width, height,        \
						 selected_index, opened)                               \
	_wp_dropdown_menu_loc(items, placeholder, item_count, width, height,       \
						  selected_index, opened, __FILE__, __LINE__)
void _wp_dropdown_menu_loc(const char **items, const char *placeholder,
						   uint32_t item_count, float width, float height,
						   int32_t *selected_index, bool *opened,
						   const char *file, int32_t line);

#define wp_dropdown_menu_wide(items, placeholder, item_count, width, height,   \
							  selected_index, opened)                          \
	_wp_dropdown_menu_loc_wide(items, placeholder, item_count, width, height,  \
							   selected_index, opened, __FILE__, __LINE__)
void _wp_dropdown_menu_loc_wide(const wchar_t **items,
								const wchar_t *placeholder, uint32_t item_count,
								float width, float height,
								int32_t *selected_index, bool *opened,
								const char *file, int32_t line);

#define wp_input_text_inl_ex(buffer, buffer_size, input_width,                 \
							 placeholder_str)                                  \
	{                                                                          \
		static wp_input_field input = {.cursor_index = 0,                      \
									   .width = input_width,                   \
									   .buf = buffer,                          \
									   .buf_size = buffer_size,                \
									   .placeholder = (char *)placeholder_str, \
									   .selected = false};                     \
		_wp_input_text_loc(&input, __FILE__, __LINE__);                        \
	}

#define wp_input_text_inl(buffer, buffer_size)                                 \
	wp_input_text_inl_ex(buffer, buffer_size,                                  \
						 (int32_t)(wp_get_current_div().aabb.size.x / 2), "")

#define wp_input_text(input) _wp_input_text_loc(input, __FILE__, __LINE__)
void _wp_input_text_loc(wp_input_field *input, const char *file, int32_t line);

#define wp_input_int(input) _wp_input_int_loc(input, __FILE__, __LINE__)
void _wp_input_int_loc(wp_input_field *input, const char *file, int32_t line);

#define wp_input_float(input) _wp_input_float_loc(input, __FILE__, __LINE__)
void _wp_input_float_loc(wp_input_field *input, const char *file, int32_t line);

void wp_input_insert_char_idx(wp_input_field *input, char c, uint32_t idx);

void wp_input_insert_str_idx(wp_input_field *input, const char *insert,
							 uint32_t len, uint32_t idx);

void wp_input_field_unselect_all(wp_input_field *input);

bool wp_input_grabbed();

void wp_div_grab(wp_div div);

void wp_div_ungrab();

bool wp_div_grabbed();

wp_div wp_get_grabbed_div();

#define wp_begin() _wp_begin_loc(__FILE__, __LINE__)
void _wp_begin_loc(const char *file, int32_t line);

void wp_end();

void wp_next_line();

vec2s wp_text_dimension(const char *str);

vec2s wp_text_dimension_ex(const char *str, float wrap_point);

vec2s wp_text_dimension_wide(const wchar_t *str);

vec2s wp_text_dimension_wide_ex(const wchar_t *str, float wrap_point);

vec2s wp_button_dimension(const char *text);

float wp_get_text_end(const char *str, float start_x);

void wp_text(const char *text);

void wp_text_wide(const wchar_t *text);

void wp_set_text_wrap(bool wrap);

wp_div wp_get_current_div();

wp_div wp_get_selected_div();

wp_div *wp_get_current_div_ptr();

wp_div *wp_get_selected_div_ptr();

void wp_set_ptr_x(float x);

void wp_set_ptr_y(float y);

void wp_set_ptr_x_absolute(float x);

void wp_set_ptr_y_absolute(float y);

float wp_get_ptr_x();

float wp_get_ptr_y();

uint32_t wp_get_display_width();

uint32_t wp_get_display_height();

void wp_push_font(wp_font *font);

void wp_pop_font();

wp_text_props wp_text_render(vec2s pos, const char *str, wp_font font,
							 wp_color color, int32_t wrap_point,
							 vec2s stop_point, bool no_render,
							 bool render_solid, int32_t start_index,
							 int32_t end_index);

wp_text_props wp_text_render_wchar(vec2s pos, const wchar_t *str, wp_font font,
								   wp_color color, int32_t wrap_point,
								   vec2s stop_point, bool no_render,
								   bool render_solid, int32_t start_index,
								   int32_t end_index);

void wp_rect_render(vec2s pos, vec2s size, wp_color color,
					wp_color border_color, float border_width,
					float corner_radius);

void wp_image_render(vec2s pos, wp_color color, wp_texture tex,
					 wp_color border_color, float border_width,
					 float corner_radius);

bool wp_point_intersects_aabb(vec2s p, wp_aabb aabb);

bool wp_aabb_intersects_aabb(wp_aabb a, wp_aabb b);

void wp_push_style_props(wp_element_props props);

void wp_pop_style_props();

bool wp_hovered(vec2s pos, vec2s size);

bool wp_area_hovered(vec2s pos, vec2s size);

wp_cursor_pos_event wp_get_mouse_move_event();

wp_mouse_button_event wp_get_mouse_button_event();

wp_scroll_event wp_get_mouse_scroll_event();

wp_key_event wp_get_key_event();

wp_char_event wp_get_char_event();

void wp_set_cull_start_x(float x);

void wp_set_cull_start_y(float y);

void wp_set_cull_end_x(float x);

void wp_set_cull_end_y(float y);

void wp_unset_cull_start_x();

void wp_unset_cull_start_y();

void wp_unset_cull_end_x();

void wp_unset_cull_end_y();

void wp_set_image_color(wp_color color);

void wp_unset_image_color();

void wp_set_current_div_scroll(float scroll);

float wp_get_current_div_scroll();

void wp_set_current_div_scroll_velocity(float scroll_velocity);

float wp_get_current_div_scroll_velocity();

void wp_set_line_height(uint32_t line_height);

uint32_t wp_get_line_height();

void wp_set_line_should_overflow(bool overflow);

void wp_set_div_hoverable(bool hoverable);

void wp_push_element_id(int64_t id);

void wp_pop_element_id();

wp_color wp_color_brightness(wp_color color, float brightness);

wp_color wp_color_alpha(wp_color color, uint8_t a);

vec4s wp_color_to_zto(wp_color color);

wp_color wp_color_from_hex(uint32_t hex);

wp_color wp_color_from_zto(vec4s zto);

void wp_image(wp_texture tex);

void wp_rect(float width, float height, wp_color color, float corner_radius);

void wp_seperator();

void wp_set_clipboard_text(const char *text);

char *wp_get_clipboard_text();

void wp_set_no_render(bool no_render);

#endif // WARP_H
