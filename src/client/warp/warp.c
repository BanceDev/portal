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

#include "warp.h"
#include <cglm/mat4.h>
#include <cglm/types-struct.h>
#include <glad/glad.h>
#include <time.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <stb_image_resize2.h>

#include <libclipboard.h>

#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#ifdef _WIN32
#define HOMEDIR "USERPROFILE"
#else
#define HOMEDIR (char *)"HOME"
#endif

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

#define WP_TRACE(...)                                                          \
	{                                                                          \
		printf("Warp: [TRACE]: ");                                             \
		printf(__VA_ARGS__);                                                   \
		printf("\n");                                                          \
	}
#ifdef WP_DEBUG
#define WP_DEBUG(...)                                                          \
	{                                                                          \
		printf("Warp: [DEBUG]: ");                                             \
		printf(__VA_ARGS__);                                                   \
		printf("\n");                                                          \
	}
#else
#define WP_DEBUG(...)
#endif // WP_DEBUG
#define WP_INFO(...)                                                           \
	{                                                                          \
		printf("Warp: [INFO]: ");                                              \
		printf(__VA_ARGS__);                                                   \
		printf("\n");                                                          \
	}
#define WP_WARN(...)                                                           \
	{                                                                          \
		printf("Warp: [WARN]: ");                                              \
		printf(__VA_ARGS__);                                                   \
		printf("\n");                                                          \
	}
#define WP_ERROR(...)                                                          \
	{                                                                          \
		printf("[WARP ERROR]: ");                                              \
		printf(__VA_ARGS__);                                                   \
		printf("\n");                                                          \
	}

#ifdef _MSC_VER
#define D_BREAK __debugbreak
#elif defined(__clang__) || defined(__GNUC__)
#define D_BREAK __builtin_trap
#else
#define D_BREAK
#endif

#ifdef _DEBUG
#define WP_ASSERT(cond, ...)                                                   \
	{                                                                          \
		if (cond) {                                                            \
		} else {                                                               \
			printf("[WARP]: Assertion failed: '");                             \
			printf(__VA_ARGS__);                                               \
			printf("' in file '%s' on line %i.\n", __FILE__, __LINE__);        \
			D_BREAK();                                                         \
		}                                                                      \
	}
#else
#define WP_ASSERT(cond, ...)
#endif // _DEBUG

#define WP_STACK_INIT_CAP 4

#define MAX_KEYS GLFW_KEY_LAST
#define MAX_MOUSE_BUTTONS GLFW_MOUSE_BUTTON_LAST
#define KEY_CALLBACK_t GLFWkeyfun
#define MOUSE_BUTTON_CALLBACK_t GLFWmousebuttonfun
#define SCROLL_CALLBACK_t GLFWscrollfun
#define CURSOR_CALLBACK_t GLFWcursorposfun
#define MAX_RENDER_BATCH 10000
#define MAX_TEX_COUNT_BATCH 32
#define MAX_KEY_CALLBACKS 4
#define MAX_MOUSE_BTTUON_CALLBACKS 4
#define MAX_SCROLL_CALLBACKS 4
#define MAX_CURSOR_POS_CALLBACKS 4

#define DJB2_INIT 5381

// -- Local Struct Defines ---
typedef struct {
	uint32_t id;
} wp_shader;

typedef struct {
	vec2 pos;				   // 8 Bytes
	vec4 border_color;		   // 16 Bytes
	float border_width;		   // 4 Bytes
	vec4 color;				   // 16 Bytes
	vec2 texcoord;			   // 8 Bytes
	float tex_index;		   // 4 Bytes
	vec2 scale;				   // 8 Bytes
	vec2 pos_px;			   // 8 Bytes
	float corner_radius;	   // 4 Bytes
	vec2 min_coord, max_coord; // 16 Bytes
} vertex_t;					   // 88 Bytes per vertex

typedef struct {
	bool keys[MAX_KEYS];
	bool keys_changed[MAX_KEYS];
} keyboard_t;

typedef struct {
	bool buttons_current[MAX_MOUSE_BUTTONS];
	bool buttons_last[MAX_MOUSE_BUTTONS];

	double xpos, ypos, xpos_last, ypos_last, xpos_delta, ypos_delta;
	bool first_mouse_press;
	double xscroll_delta, yscroll_delta;
} mouse_t;

typedef struct {
	bool is_dragging;
	vec2s start_cursor_pos;
	float start_scroll;
} wp_drag_state;

// State of input
typedef struct {
	keyboard_t keyboard;
	mouse_t mouse;

	// List of callbacks (user defined)
	KEY_CALLBACK_t key_cbs[MAX_KEY_CALLBACKS];
	MOUSE_BUTTON_CALLBACK_t mouse_button_cbs[MAX_MOUSE_BTTUON_CALLBACKS];
	SCROLL_CALLBACK_t scroll_cbs[MAX_SCROLL_CALLBACKS];
	CURSOR_CALLBACK_t cursor_pos_cbs[MAX_CURSOR_POS_CALLBACKS];

	uint32_t key_cb_count, mouse_button_cb_count, scroll_cb_count,
		cursor_pos_cb_count;
} wp_input_state;

// State of the batch renderer
typedef struct {
	wp_shader shader;
	uint32_t vao, vbo, ibo;
	uint32_t vert_count;
	vertex_t *verts;
	vec4s vert_pos[4];
	wp_texture textures[MAX_TEX_COUNT_BATCH];
	uint32_t tex_index, tex_count, index_count;
} wp_render_state;

typedef struct {
	wp_element_props *data;
	uint32_t count, cap;
} props_stack_t;

typedef struct {
	bool init;

	// Window
	uint32_t dsp_w, dsp_h;
	void *window_handle;

	wp_render_state render;
	wp_input_state input;
	wp_theme theme;

	wp_div current_div, prev_div;
	int32_t current_line_height, prev_line_height;
	vec2s pos_ptr, prev_pos_ptr;

	// Pushable variables
	wp_font *font_stack, *prev_font_stack;
	wp_element_props div_props, prev_props_stack;
	wp_color image_color_stack;
	int64_t element_id_stack;

	props_stack_t props_stack;

	// Event references
	wp_key_event key_ev;
	wp_mouse_button_event mb_ev;
	wp_cursor_pos_event cp_ev;
	wp_scroll_event scr_ev;
	wp_char_event ch_ev;

	vec2s cull_start, cull_end;

	wp_texture tex_arrow_down, tex_tick;

	bool text_wrap, line_overflow, div_hoverable, input_grabbed;

	uint64_t active_element_id;

	float *scroll_velocity_ptr;
	float *scroll_ptr;

	wp_div selected_div, selected_div_tmp, scrollbar_div, grabbed_div;

	uint32_t drawcalls;

	bool entered_div;

	bool div_velocity_accelerating;

	float last_time, delta_time;
	clipboard_c *clipboard;

	bool renderer_render;

	wp_drag_state drag_state;
} wp_state;

typedef enum { INPUT_INT = 0, INPUT_FLOAT, INPUT_TEXT } input_field_type_t;

// Static object to retrieve state data during runtime
static wp_state state;

// --- Renderer ---
static uint32_t shader_create(GLenum type, const char *src);
static wp_shader shader_prg_create(const char *vert_src, const char *frag_src);
static void shader_set_mat(wp_shader prg, const char *name, mat4 mat);
static void set_projection_matrix();
static void renderer_init();
static void renderer_flush();
static void renderer_begin();

static wp_text_props text_render_simple(vec2s pos, const char *text,
										wp_font font, wp_color font_color,
										bool no_render);
static wp_text_props text_render_simple_wide(vec2s pos, const wchar_t *text,
											 wp_font font, wp_color font_color,
											 bool no_render);

static wp_clickable_state button_ex(const char *file, int32_t line, vec2s pos,
									vec2s size, wp_element_props props,
									wp_color color, float border_width,
									bool click_color, bool hover_color,
									vec2s hitbox_override);
static wp_clickable_state button(const char *file, int32_t line, vec2s pos,
								 vec2s size, wp_element_props props,
								 wp_color color, float border_width,
								 bool click_color, bool hover_color);
static wp_clickable_state div_container(vec2s pos, vec2s size,
										wp_element_props props, wp_color color,
										float border_width, bool click_color,
										bool hover_color);
static void next_line_on_overflow(vec2s size, float xoffset);
static bool item_should_cull(wp_aabb item);
static void draw_scrollbar_on(wp_div *div);

static void input_field(wp_input_field *input, input_field_type_t type,
						const char *file, int32_t line);

wp_font load_font(const char *filepath, uint32_t pixelsize, uint32_t tex_width,
				  uint32_t tex_height, uint32_t line_gap_add);
static wp_font get_current_font();

static wp_clickable_state button_element_loc(void *text, const char *file,
											 int32_t line, bool wide);
static wp_clickable_state button_fixed_element_loc(void *text, float width,
												   float height,
												   const char *file,
												   int32_t line, bool wide);
static wp_clickable_state checkbox_element_loc(void *text, bool *val,
											   wp_color tick_color,
											   wp_color tex_color,
											   const char *file, int32_t line,
											   bool wide);
static void dropdown_menu_item_loc(void **items, void *placeholder,
								   uint32_t item_count, float width,
								   float height, int32_t *selected_index,
								   bool *opened, const char *file, int32_t line,
								   bool wide);
static int32_t menu_item_list_item_loc(void **items, uint32_t item_count,
									   int32_t selected_index,
									   wp_menu_item_callback per_cb,
									   bool vertical, const char *file,
									   int32_t line, bool wide);

// --- Utility ---
static int32_t get_max_char_height_font(wp_font font);

static void remove_i_str(char *str, int32_t index);
static void remove_substr_str(char *str, int start_index, int end_index);
static void insert_i_str(char *str, char ch, int32_t index);
static void insert_str_str(char *source, const char *insert, int32_t index);
static void substr_str(const char *str, int start_index, int end_index,
					   char *substring);

static int map_vals(int value, int from_min, int from_max, int to_min,
					int to_max);

// --- Input ---
static void glfw_key_callback(GLFWwindow *window, int32_t key, int scancode,
							  int action, int mods);
static void glfw_mouse_button_callback(GLFWwindow *window, int32_t button,
									   int action, int mods);
static void glfw_scroll_callback(GLFWwindow *window, double xoffset,
								 double yoffset);
static void glfw_cursor_callback(GLFWwindow *window, double xpos, double ypos);
static void glfw_char_callback(GLFWwindow *window, uint32_t charcode);

static void update_input();
static void clear_events();

static uint64_t djb2_hash(uint64_t hash, const void *buf, size_t size);

static void props_stack_create(props_stack_t *stack);
static void props_stack_resize(props_stack_t *stack, uint32_t newcap);
static void props_stack_push(props_stack_t *stack, wp_element_props props);
static wp_element_props props_stack_pop(props_stack_t *stack);
static wp_element_props props_stack_peak(props_stack_t *stack);
static bool props_stack_empty(props_stack_t *stack);

static wp_element_props get_props_for(wp_element_props props);

// --- Static Functions ---
uint32_t shader_create(GLenum type, const char *src) {
	// Create && compile the shader with opengl
	uint32_t shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	// Check for compilation errors
	int32_t compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled) {
		WP_ERROR("Failed to compile %s shader.",
				 type == GL_VERTEX_SHADER ? "vertex" : "fragment");
		char info[512];
		glGetShaderInfoLog(shader, 512, NULL, info);
		WP_INFO("%s", info);
		glDeleteShader(shader);
	}
	return shader;
}

wp_shader shader_prg_create(const char *vert_src, const char *frag_src) {
	// Creating vertex & fragment shader with the shader API
	uint32_t vertex_shader = shader_create(GL_VERTEX_SHADER, vert_src);
	uint32_t fragment_shader = shader_create(GL_FRAGMENT_SHADER, frag_src);

	// Creating & linking the shader program with OpenGL
	wp_shader prg;
	prg.id = glCreateProgram();
	glAttachShader(prg.id, vertex_shader);
	glAttachShader(prg.id, fragment_shader);
	glLinkProgram(prg.id);

	// Check for linking errors
	int32_t linked;
	glGetProgramiv(prg.id, GL_LINK_STATUS, &linked);

	if (!linked) {
		WP_ERROR("Failed to link shader program.");
		char info[512];
		glGetProgramInfoLog(prg.id, 512, NULL, info);
		WP_INFO("%s", info);
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		glDeleteProgram(prg.id);
		return prg;
	}

	// Delete the shaders after
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return prg;
}

void shader_set_mat(wp_shader prg, const char *name, mat4 mat) {
	glUniformMatrix4fv(glGetUniformLocation(prg.id, name), 1, GL_FALSE, mat[0]);
}

void set_projection_matrix() {
	float left = 0.0f;
	float right = state.dsp_w;
	float bottom = state.dsp_h;
	float top = 0.0f;
	float near = 0.1f;
	float far = 100.0f;

	// Create the orthographic projection matrix
	mat4 orthoMatrix = GLM_MAT4_IDENTITY_INIT;
	orthoMatrix[0][0] = 2.0f / (right - left);
	orthoMatrix[1][1] = 2.0f / (top - bottom);
	orthoMatrix[2][2] = -1;
	orthoMatrix[3][0] = -(right + left) / (right - left);
	orthoMatrix[3][1] = -(top + bottom) / (top - bottom);

	shader_set_mat(state.render.shader, "u_proj", orthoMatrix);
}

void renderer_init() {
	// OpenGL Setup
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	state.render.vert_count = 0;
	state.render.verts =
		(vertex_t *)malloc(sizeof(vertex_t) * MAX_RENDER_BATCH * 4);

	/* Creating vertex array & vertex buffer for the batch renderer */
	glCreateVertexArrays(1, &state.render.vao);
	glBindVertexArray(state.render.vao);

	glCreateBuffers(1, &state.render.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, state.render.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_t) * MAX_RENDER_BATCH * 4, NULL,
				 GL_DYNAMIC_DRAW);

	uint32_t *indices =
		(uint32_t *)malloc(sizeof(uint32_t) * MAX_RENDER_BATCH * 6);

	uint32_t offset = 0;
	for (uint32_t i = 0; i < MAX_RENDER_BATCH * 6; i += 6) {
		indices[i + 0] = offset + 0;
		indices[i + 1] = offset + 1;
		indices[i + 2] = offset + 2;

		indices[i + 3] = offset + 2;
		indices[i + 4] = offset + 3;
		indices[i + 5] = offset + 0;
		offset += 4;
	}
	glCreateBuffers(1, &state.render.ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.render.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
				 MAX_RENDER_BATCH * 6 * sizeof(uint32_t), indices,
				 GL_STATIC_DRAW);

	free(indices);
	// Setting the vertex layout
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t), NULL);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t)offsetof(vertex_t, border_color));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t)offsetof(vertex_t, border_width));
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t)offsetof(vertex_t, color));
	glEnableVertexAttribArray(3);

	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t *)offsetof(vertex_t, texcoord));
	glEnableVertexAttribArray(4);

	glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t *)offsetof(vertex_t, tex_index));
	glEnableVertexAttribArray(5);

	glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t *)offsetof(vertex_t, scale));
	glEnableVertexAttribArray(6);

	glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t *)offsetof(vertex_t, pos_px));
	glEnableVertexAttribArray(7);

	glVertexAttribPointer(
		8, 1, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
		(void *)(intptr_t *)offsetof(vertex_t, corner_radius));
	glEnableVertexAttribArray(8);

	glVertexAttribPointer(10, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t *)offsetof(vertex_t, min_coord));
	glEnableVertexAttribArray(10);

	glVertexAttribPointer(11, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t),
						  (void *)(intptr_t *)offsetof(vertex_t, max_coord));
	glEnableVertexAttribArray(11);

	// Creating the shader for the batch renderer
	const char *vert_src =
		"#version 450 core\n"
		"layout (location = 0) in vec2 a_pos;\n"
		"layout (location = 1) in vec4 a_border_color;\n"
		"layout (location = 2) in float a_border_width;\n"
		"layout (location = 3) in vec4 a_color;\n"
		"layout (location = 4) in vec2 a_texcoord;\n"
		"layout (location = 5) in float a_tex_index;\n"
		"layout (location = 6) in vec2 a_scale;\n"
		"layout (location = 7) in vec2 a_pos_px;\n"
		"layout (location = 8) in float a_corner_radius;\n"
		"layout (location = 10) in vec2 a_min_coord;\n"
		"layout (location = 11) in vec2 a_max_coord;\n"

		"uniform mat4 u_proj;\n"
		"out vec4 v_border_color;\n"
		"out float v_border_width;\n"
		"out vec4 v_color;\n"
		"out vec2 v_texcoord;\n"
		"out float v_tex_index;\n"
		"flat out vec2 v_scale;\n"
		"flat out vec2 v_pos_px;\n"
		"flat out float v_is_gradient;\n"
		"out float v_corner_radius;\n"
		"out vec2 v_min_coord;\n"
		"out vec2 v_max_coord;\n"

		"void main() {\n"
		"v_color = a_color;\n"
		"v_texcoord = a_texcoord;\n"
		"v_tex_index = a_tex_index;\n"
		"v_border_color = a_border_color;\n"
		"v_border_width = a_border_width;\n"
		"v_scale = a_scale;\n"
		"v_pos_px = a_pos_px;\n"
		"v_corner_radius = a_corner_radius;\n"
		"v_min_coord = a_min_coord;\n"
		"v_max_coord = a_max_coord;\n"
		"gl_Position = u_proj * vec4(a_pos.x, a_pos.y, 0.0f, 1.0);\n"
		"}\n";

	const char *frag_src =
		"#version 450 core\n"
		"out vec4 o_color;\n"
		"in vec4 v_color;\n"
		"in float v_tex_index;\n"
		"in vec4 v_border_color;\n"
		"in float v_border_width;\n"
		"in vec2 v_texcoord;\n"
		"flat in vec2 v_scale;\n"
		"flat in vec2 v_pos_px;\n"
		"in float v_corner_radius;\n"
		"uniform sampler2D u_textures[32];\n"
		"uniform vec2 u_screen_size;\n"
		"in vec2 v_min_coord;\n"
		"in vec2 v_max_coord;\n"

		"float rounded_box_sdf(vec2 center_pos, vec2 size, float radius) {\n"
		"    return length(max(abs(center_pos)-size+radius,0.0))-radius;\n"
		"}\n"

		"void main() {\n"
		"     if(u_screen_size.y - gl_FragCoord.y < v_min_coord.y && "
		"v_min_coord.y != -1) {\n"
		"         discard;\n"
		"     }\n"
		"     if(u_screen_size.y - gl_FragCoord.y > v_max_coord.y && "
		"v_max_coord.y != -1) {\n"
		"         discard;\n"
		"     }\n"
		"     if ((gl_FragCoord.x < v_min_coord.x && v_min_coord.x != -1) || "
		"(gl_FragCoord.x > v_max_coord.x && v_max_coord.x != -1)) {\n"
		"         discard;\n"
		"     }\n"
		"     vec2 size = v_scale;\n"
		"     vec4 opaque_color, display_color;\n"
		"     if(v_tex_index == -1) {\n"
		"       opaque_color = v_color;\n"
		"     } else {\n"
		"       opaque_color = texture(u_textures[int(v_tex_index)], "
		"v_texcoord) * v_color;\n"
		"     }\n"
		"     if(v_corner_radius != 0.0f) {"
		"       display_color = opaque_color;\n"
		"       vec2 location = vec2(v_pos_px.x, -v_pos_px.y);\n"
		"       location.y += u_screen_size.y - size.y;\n"
		"       float edge_softness = 1.0f;\n"
		"       float radius = v_corner_radius * 2.0f;\n"
		"       float distance = rounded_box_sdf(gl_FragCoord.xy - location - "
		"(size/2.0f), size / 2.0f, radius);\n"
		"       float smoothed_alpha = 1.0f-smoothstep(0.0f, edge_softness * "
		"2.0f,distance);\n"
		"       vec3 fill_color;\n"
		"       if(v_border_width != 0.0f) {\n"
		"           vec2 location_border = vec2(location.x + v_border_width, "
		"location.y + v_border_width);\n"
		"           vec2 size_border = vec2(size.x - v_border_width * 2, "
		"size.y - v_border_width * 2);\n"
		"           float distance_border = rounded_box_sdf(gl_FragCoord.xy - "
		"location_border - (size_border / 2.0f), size_border / 2.0f, radius);\n"
		"           if(distance_border <= 0.0f) {\n"
		"               fill_color = display_color.xyz;\n"
		"           } else {\n"
		"               fill_color = v_border_color.xyz;\n"
		"           }\n"
		"       } else {\n"
		"           fill_color = display_color.xyz;\n"
		"       }\n"
		"       if(v_border_width != 0.0f)\n"
		"         o_color =  mix(vec4(0.0f, 0.0f, 0.0f, 0.0f), "
		"vec4(fill_color, smoothed_alpha), smoothed_alpha);\n"
		"       else\n"
		"         o_color = mix(vec4(0.0f, 0.0f, 0.0f, 0.0f), vec4(fill_color, "
		"display_color.a), smoothed_alpha);\n"
		"     } else {\n"
		"       vec4 fill_color = opaque_color;\n"
		"       if(v_border_width != 0.0f) {\n"
		"           vec2 location = vec2(v_pos_px.x, -v_pos_px.y);\n"
		"           location.y += u_screen_size.y - size.y;\n"
		"           vec2 location_border = vec2(location.x + v_border_width, "
		"location.y + v_border_width);\n"
		"           vec2 size_border = vec2(v_scale.x - v_border_width * 2, "
		"v_scale.y - v_border_width * 2);\n"
		"           float distance_border = rounded_box_sdf(gl_FragCoord.xy - "
		"location_border - (size_border / 2.0f), size_border / 2.0f, "
		"v_corner_radius);\n"
		"           if(distance_border > 0.0f) {\n"
		"               fill_color = v_border_color;\n"
		"}\n"
		"       }\n"
		"       o_color = fill_color;\n"
		" }\n"
		"}\n";
	state.render.shader = shader_prg_create(vert_src, frag_src);

	// initializing vertex position data
	state.render.vert_pos[0] = (vec4s){-0.5f, -0.5f, 0.0f, 1.0f};
	state.render.vert_pos[1] = (vec4s){0.5f, -0.5f, 0.0f, 1.0f};
	state.render.vert_pos[2] = (vec4s){0.5f, 0.5f, 0.0f, 1.0f};
	state.render.vert_pos[3] = (vec4s){-0.5f, 0.5f, 0.0f, 1.0f};

	// Populating the textures array in the shader with texture ids
	int32_t tex_slots[MAX_TEX_COUNT_BATCH];
	for (uint32_t i = 0; i < MAX_TEX_COUNT_BATCH; i++)
		tex_slots[i] = i;

	glUseProgram(state.render.shader.id);
	set_projection_matrix();
	glUniform1iv(glGetUniformLocation(state.render.shader.id, "u_textures"),
				 MAX_TEX_COUNT_BATCH, tex_slots);
}

void renderer_begin() {
	state.render.vert_count = 0;
	state.render.index_count = 0;
	state.render.tex_index = 0;
	state.render.tex_count = 0;
	state.drawcalls = 0;
}

void renderer_flush() {
	if (state.render.vert_count <= 0)
		return;

	// Bind the vertex buffer & shader set the vertex data, bind the textures &
	// draw
	glUseProgram(state.render.shader.id);
	glBindBuffer(GL_ARRAY_BUFFER, state.render.vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0,
					sizeof(vertex_t) * state.render.vert_count,
					state.render.verts);

	for (uint32_t i = 0; i < state.render.tex_count; i++) {
		glBindTextureUnit(i, state.render.textures[i].id);
		state.drawcalls++;
	}

	vec2s renderSize = (vec2s){(float)state.dsp_w, (float)state.dsp_h};
	glUniform2fv(glGetUniformLocation(state.render.shader.id, "u_screen_size"),
				 1, (float *)renderSize.raw);
	glBindVertexArray(state.render.vao);
	glDrawElements(GL_TRIANGLES, state.render.index_count, GL_UNSIGNED_INT,
				   NULL);
}

wp_text_props text_render_simple(vec2s pos, const char *text, wp_font font,
								 wp_color font_color, bool no_render) {
	return wp_text_render(pos, text, font, font_color, -1, (vec2s){-1, -1},
						  no_render, false, -1, -1);
}
wp_text_props text_render_simple_wide(vec2s pos, const wchar_t *text,
									  wp_font font, wp_color font_color,
									  bool no_render) {
	return wp_text_render_wchar(pos, text, font, font_color, -1,
								(vec2s){-1, -1}, no_render, false, -1, -1);
}

wp_clickable_state button(const char *file, int32_t line, vec2s pos, vec2s size,
						  wp_element_props props, wp_color color,
						  float border_width, bool click_color,
						  bool hover_color) {
	return button_ex(file, line, pos, size, props, color, border_width,
					 click_color, hover_color, (vec2s){-1, -1});
}
wp_clickable_state button_ex(const char *file, int32_t line, vec2s pos,
							 vec2s size, wp_element_props props, wp_color color,
							 float border_width, bool click_color,
							 bool hover_color, vec2s hitbox_override) {
	uint64_t id = DJB2_INIT;
	id = djb2_hash(id, file, strlen(file));
	id = djb2_hash(id, &line, sizeof(line));
	if (state.element_id_stack != -1) {
		id = djb2_hash(id, &state.element_id_stack,
					   sizeof(state.element_id_stack));
	}

	if (item_should_cull((wp_aabb){.pos = pos, .size = size})) {
		return WP_IDLE;
	}

	wp_color hover_color_rgb =
		hover_color
			? (props.hover_color.a == 0.0f ? wp_color_brightness(color, 1.2)
										   : props.hover_color)
			: color;
	wp_color held_color_rgb =
		click_color ? wp_color_brightness(color, 1.3) : color;

	bool is_hovered = wp_hovered(
		pos, (vec2s){hitbox_override.x != -1 ? hitbox_override.x : size.x,
					 hitbox_override.y != -1 ? hitbox_override.y : size.y});
	if (state.active_element_id == 0) {
		if (is_hovered && wp_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT)) {
			state.active_element_id = id;
		}
	} else if (state.active_element_id == id) {
		if (is_hovered && wp_mouse_button_up(GLFW_MOUSE_BUTTON_LEFT)) {
			wp_rect_render(pos, size, hover_color_rgb, props.border_color,
						   border_width, props.corner_radius);
			state.active_element_id = 0;
			return WP_CLICKED;
		}
	}
	if (is_hovered && wp_mouse_button_up(GLFW_MOUSE_BUTTON_LEFT)) {
		state.active_element_id = 0;
	}
	if (is_hovered && wp_mouse_button_held(GLFW_MOUSE_BUTTON_LEFT)) {
		wp_rect_render(pos, size, held_color_rgb, props.border_color,
					   border_width, props.corner_radius);
		return WP_HELD;
	}
	if (is_hovered && (!wp_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT) &&
					   !wp_mouse_button_held(GLFW_MOUSE_BUTTON_LEFT))) {
		wp_rect_render(pos, size, hover_color ? hover_color_rgb : color,
					   props.border_color, border_width, props.corner_radius);
		return WP_HOVERED;
	}
	wp_rect_render(pos, size, color, props.border_color, border_width,
				   props.corner_radius);
	return WP_IDLE;
}
wp_clickable_state div_container(vec2s pos, vec2s size, wp_element_props props,
								 wp_color color, float border_width,
								 bool click_color, bool hover_color) {
	if (item_should_cull((wp_aabb){.pos = pos, .size = size})) {
		return WP_IDLE;
	}

	wp_color hover_color_rgb =
		hover_color
			? (props.hover_color.a == 0.0f ? wp_color_brightness(color, 1.5)
										   : props.hover_color)
			: color;
	wp_color held_color_rgb =
		click_color ? wp_color_brightness(color, 1.8) : color;

	bool is_hovered = wp_hovered(pos, size);
	if (is_hovered && wp_mouse_button_up(GLFW_MOUSE_BUTTON_LEFT)) {
		wp_rect_render(pos, size, hover_color_rgb, props.border_color,
					   border_width, props.corner_radius);
		return WP_CLICKED;
	}
	if (is_hovered && wp_mouse_button_held(GLFW_MOUSE_BUTTON_LEFT)) {
		wp_rect_render(pos, size, held_color_rgb, props.border_color,
					   border_width, props.corner_radius);
		return WP_HELD;
	}
	if (is_hovered && (!wp_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT) &&
					   !wp_mouse_button_held(GLFW_MOUSE_BUTTON_LEFT))) {
		wp_rect_render(pos, size, hover_color ? hover_color_rgb : color,
					   props.border_color, border_width, props.corner_radius);
		return WP_HOVERED;
	}
	wp_rect_render(pos, size, color, props.border_color, border_width,
				   props.corner_radius);
	return WP_IDLE;
}

void next_line_on_overflow(vec2s size, float xoffset) {
	if (!state.line_overflow)
		return;

	// If the object does not fit in the area of the current div, advance to the
	// next line
	if (state.pos_ptr.x - state.current_div.aabb.pos.x + size.x >
		state.current_div.aabb.size.x) {
		state.pos_ptr.y += state.current_line_height;
		state.pos_ptr.x = state.current_div.aabb.pos.x + xoffset;
		state.current_line_height = 0;
	}
	if (size.y > state.current_line_height) {
		state.current_line_height = size.y;
	}
}

bool item_should_cull(wp_aabb item) {
	bool intersect = true;
	wp_aabb window = (wp_aabb){.pos = (vec2s){0, 0},
							   .size = (vec2s){state.dsp_w, state.dsp_h}};
	if (item.size.x == -1 || item.size.y == -1) {
		item.size.x = state.dsp_w;
		item.size.y = get_current_font().font_size;
	}
	if (item.pos.x + item.size.x <= window.pos.x ||
		item.pos.x >= window.pos.x + window.size.x)
		intersect = false;

	if (item.pos.y + item.size.y <= window.pos.y ||
		item.pos.y >= window.pos.y + window.size.y)
		intersect = false;

	return !intersect && state.current_div.id == state.scrollbar_div.id;

	return false;
}

void draw_scrollbar_on(wp_div *div) {
	wp_next_line();
	if (state.current_div.id == div->id) {
		state.scrollbar_div = *div;
		wp_div *selected = div;
		float scroll = *state.scroll_ptr;
		wp_element_props props = get_props_for(state.theme.scrollbar_props);

		selected->total_area.x = state.pos_ptr.x;
		selected->total_area.y =
			state.pos_ptr.y + state.div_props.corner_radius;

		if (*state.scroll_ptr < -((div->total_area.y - *state.scroll_ptr) -
								  div->aabb.pos.y - div->aabb.size.y) &&
			*state.scroll_velocity_ptr < 0 && state.theme.div_smooth_scroll) {
			*state.scroll_velocity_ptr = 0;
			*state.scroll_ptr = -((div->total_area.y - *state.scroll_ptr) -
								  div->aabb.pos.y - div->aabb.size.y);
		}

		float total_area = selected->total_area.y - scroll;
		float visible_area = selected->aabb.size.y + selected->aabb.pos.y;
		if (total_area > visible_area) {
			const float min_scrollbar_height = 20;

			float area_mapped = visible_area / total_area;
			float scroll_mapped = (-1 * scroll) / total_area;
			float scrollbar_height = MAX(
				(selected->aabb.size.y * area_mapped - props.margin_top * 2),
				min_scrollbar_height);

			wp_aabb scrollbar_area = (wp_aabb){
				.pos =
					(vec2s){selected->aabb.pos.x + selected->aabb.size.x -
								state.theme.scrollbar_width -
								props.margin_right - state.div_props.padding -
								state.div_props.border_width,
							MIN((selected->aabb.pos.y +
								 selected->aabb.size.y * scroll_mapped +
								 props.margin_top + state.div_props.padding +
								 state.div_props.border_width +
								 state.div_props.corner_radius),
								visible_area - scrollbar_height)},
				.size = (vec2s){state.theme.scrollbar_width,
								scrollbar_height -
									state.div_props.border_width * 2 -
									state.div_props.corner_radius * 2},
			};

			vec2s cursorpos = (vec2s){wp_get_mouse_x(), wp_get_mouse_y()};
			if (wp_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT) &&
				wp_hovered(scrollbar_area.pos, scrollbar_area.size)) {
				state.drag_state.is_dragging = true;
				state.drag_state.start_cursor_pos = cursorpos;
				state.drag_state.start_scroll = *state.scroll_ptr;
			}
			if (state.drag_state.is_dragging) {
				float cursor_delta =
					(cursorpos.y - state.drag_state.start_cursor_pos.y);
				float new_scroll = state.drag_state.start_scroll -
								   cursor_delta * (total_area / visible_area);
				*state.scroll_ptr = new_scroll;

				if (*state.scroll_ptr > 0) {
					*state.scroll_ptr = 0;
				} else if (*state.scroll_ptr < -(total_area - visible_area)) {
					*state.scroll_ptr = -(total_area - visible_area);
				}
			}
			if (wp_mouse_button_up(GLFW_MOUSE_BUTTON_LEFT)) {
				state.drag_state.is_dragging = false;
			}

			wp_rect_render(scrollbar_area.pos, scrollbar_area.size, props.color,
						   props.border_color, props.border_width,
						   props.corner_radius);
		}
	}
}

void input_field(wp_input_field *input, input_field_type_t type,
				 const char *file, int32_t line) {
	if (!input->buf)
		return;

	if (!input->_init) {
		wp_input_field_unselect_all(input);
		input->_init = true;
	}

	wp_element_props props = get_props_for(state.theme.inputfield_props);
	wp_font font = get_current_font();

	state.pos_ptr.x += props.margin_left;
	state.pos_ptr.y += props.margin_top;

	float wrap_point = state.pos_ptr.x + input->width - props.padding;

	if (input->selected) {
		if (wp_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT) &&
			(wp_get_mouse_x_delta() == 0 && wp_get_mouse_y_delta() == 0)) {
			wp_text_props selected_props =
				wp_text_render((vec2s){state.pos_ptr.x + props.padding,
									   state.pos_ptr.y + props.padding},
							   input->buf, font, WP_NO_COLOR, wrap_point,
							   (vec2s){wp_get_mouse_x(), wp_get_mouse_y()},
							   true, false, -1, -1);
			input->cursor_index = selected_props.rendered_count;
			wp_input_field_unselect_all(input);
			input->mouse_selection_end = input->cursor_index;
			input->mouse_selection_start = input->cursor_index;
		} else if (wp_mouse_button_held(GLFW_MOUSE_BUTTON_LEFT) &&
				   (wp_get_mouse_x_delta() != 0 ||
					wp_get_mouse_y_delta() != 0)) {
			if (input->mouse_dir == 0) {
				input->mouse_dir = (wp_get_mouse_x_delta() < 0) ? -1 : 1;
				input->mouse_selection_end = input->cursor_index;
				input->mouse_selection_start = input->cursor_index;
			}
			wp_text_props selected_props =
				wp_text_render((vec2s){state.pos_ptr.x + props.padding,
									   state.pos_ptr.y + props.padding},
							   input->buf, font, WP_NO_COLOR, wrap_point,
							   (vec2s){wp_get_mouse_x(), wp_get_mouse_y()},
							   true, false, -1, -1);

			input->cursor_index = selected_props.rendered_count;

			if (input->mouse_dir == -1)
				input->mouse_selection_start = input->cursor_index;
			else if (input->mouse_dir == 1)
				input->mouse_selection_end = input->cursor_index;

			input->selection_start = input->mouse_selection_start;
			input->selection_end = input->mouse_selection_end;

			if (input->mouse_selection_start == input->mouse_selection_end) {
				input->mouse_dir = (wp_get_mouse_x_delta() < 0) ? -1 : 1;
			}
		} else if (wp_mouse_button_up(GLFW_MOUSE_BUTTON_LEFT)) {
			input->mouse_dir = 0;
		}
		if (wp_get_char_event().happened && wp_get_char_event().charcode >= 0 &&
			wp_get_char_event().charcode <= 127 &&
			strlen(input->buf) + 1 <= input->buf_size &&
			(input->max_chars ? strlen(input->buf) + 1 <= input->max_chars
							  : true)) {
			if (input->insert_override_callback) {
				input->insert_override_callback(input);
			} else {
				if (input->selection_start != -1) {
					int start = input->selection_dir != 0
									? input->selection_start
									: input->selection_start - 1;
					int end = input->selection_end;

					remove_substr_str(input->buf, start, end);

					input->cursor_index = input->selection_start;
					wp_input_field_unselect_all(input);
				}
				wp_input_insert_char_idx(input, wp_get_char_event().charcode,
										 input->cursor_index++);
			}
		}

		if (wp_get_key_event().happened && wp_get_key_event().pressed) {
			switch (wp_get_key_event().keycode) {
			case GLFW_KEY_BACKSPACE: {
				if (input->selection_start != -1) {
					int start = input->selection_dir != 0
									? input->selection_start
									: input->selection_start - 1;
					int end = input->selection_end;

					remove_substr_str(input->buf, start, end);

					input->cursor_index = input->selection_start;
					wp_input_field_unselect_all(input);
				} else {
					if (input->cursor_index - 1 < 0)
						break;
					remove_i_str(input->buf, input->cursor_index - 1);
					input->cursor_index--;
				}
				break;
			}
			case GLFW_KEY_LEFT: {
				if (input->cursor_index - 1 < 0) {
					if (!wp_key_held(GLFW_KEY_LEFT_SHIFT))
						wp_input_field_unselect_all(input);
					break;
				}
				if (wp_key_held(GLFW_KEY_LEFT_SHIFT)) {
					if (input->selection_end == -1) {
						input->selection_end = input->cursor_index - 1;
						input->selection_dir = -1;
					}
					input->cursor_index--;
					if (input->selection_dir == 1) {
						if (input->cursor_index != input->selection_start) {
							input->selection_end = input->cursor_index - 1;
						} else {
							wp_input_field_unselect_all(input);
						}
					} else {
						input->selection_start = input->cursor_index;
					}
				} else {
					if (input->selection_end == -1)
						input->cursor_index--;
					wp_input_field_unselect_all(input);
				}
				break;
			}
			case GLFW_KEY_RIGHT: {
				if (input->cursor_index + 1 > strlen(input->buf)) {
					if (!wp_key_held(GLFW_KEY_LEFT_SHIFT))
						wp_input_field_unselect_all(input);
					break;
				}
				if (wp_key_held(GLFW_KEY_LEFT_SHIFT)) {
					if (input->selection_start == -1) {
						input->selection_start = input->cursor_index;
						input->selection_dir = 1;
					}
					if (input->selection_dir == -1) {
						input->cursor_index++;
						if (input->cursor_index - 1 != input->selection_end) {
							input->selection_start = input->cursor_index;
						} else {
							wp_input_field_unselect_all(input);
						}
					} else {
						input->selection_end = input->cursor_index;
						input->cursor_index++;
					}
				} else {
					if (input->selection_end == -1)
						input->cursor_index++;
					wp_input_field_unselect_all(input);
				}
				break;
			}
			case GLFW_KEY_ENTER: {
				// TODO: There is a bug with the input cursor when inserting new
				// line characters
				/*
							insert_i_str(input->buf, '\n',
				   input->cursor_index++);
							*/
				break;
			}
			case GLFW_KEY_TAB: {
				if (strlen(input->buf) + 1 <= input->buf_size &&
					(input->max_chars
						 ? strlen(input->buf) + 1 <= input->max_chars
						 : true)) {
					for (uint32_t i = 0; i < 2; i++) {
						insert_i_str(input->buf, ' ', input->cursor_index++);
					}
				}
				break;
			}
			case GLFW_KEY_A: {
				if (!wp_key_held(GLFW_KEY_LEFT_CONTROL))
					break;
				bool selected_all = input->selection_start == 0 &&
									input->selection_end == strlen(input->buf);
				if (selected_all) {
					wp_input_field_unselect_all(input);
				} else {
					input->selection_start = 0;
					input->selection_end = strlen(input->buf);
				}
				break;
			}
			case GLFW_KEY_C: {
				if (!wp_key_held(GLFW_KEY_LEFT_CONTROL))
					break;
				char selection[strlen(input->buf)];
				memset(selection, 0, strlen(input->buf));
				substr_str(input->buf, input->selection_start,
						   input->selection_end, selection);

				clipboard_set_text(state.clipboard, selection);
				break;
			}
			case GLFW_KEY_V: {
				if (!wp_key_held(GLFW_KEY_LEFT_CONTROL))
					break;
				int32_t length;
				const char *clipboard_content =
					clipboard_text_ex(state.clipboard, &length, LCB_CLIPBOARD);
				if (strlen(input->buf) + length > input->buf_size ||
					(input->max_chars
						 ? strlen(input->buf) + length > input->max_chars
						 : false))
					break;

				wp_input_insert_str_idx(input, clipboard_content, length,
										input->cursor_index);
				input->cursor_index += strlen(clipboard_content);
				break;
			}
			case GLFW_KEY_X: {
				if (!wp_key_held(GLFW_KEY_LEFT_CONTROL))
					break;
				char selection[strlen(input->buf)];
				memset(selection, 0, strlen(input->buf));
				substr_str(input->buf, input->selection_start,
						   input->selection_end, selection);

				clipboard_set_text(state.clipboard, selection);

				int start = input->selection_dir != 0
								? input->selection_start
								: input->selection_start - 1;
				remove_substr_str(input->buf, start, input->selection_end);
				input->cursor_index = input->selection_start;
				wp_input_field_unselect_all(input);
				break;
			}
			default: {
				break;
			}
			}
		}
		if (input->key_callback) {
			input->key_callback(input);
		}
	}

	wp_text_props textprops =
		wp_text_render((vec2s){state.pos_ptr.x + props.padding,
							   state.pos_ptr.y + props.padding},
					   input->buf, font, WP_NO_COLOR, wrap_point,
					   (vec2s){-1, -1}, true, false, -1, -1);

	if (!input->retain_height) {
		input->height =
			(input->start_height)
				? MAX(input->start_height, textprops.height)
				: (textprops.height ? textprops.height
									: get_max_char_height_font(font));
	} else {
		input->height = (input->start_height) ? input->start_height
											  : get_max_char_height_font(font);
	}

	next_line_on_overflow((vec2s){input->width + props.padding * 2.0f +
									  props.margin_right + props.margin_left,
								  input->height + props.padding * 2.0f +
									  props.margin_bottom + props.margin_top},
						  state.div_props.border_width);

	wp_aabb input_aabb =
		(wp_aabb){.pos = state.pos_ptr,
				  .size = (vec2s){input->width + props.padding * 2.0f,
								  input->height + props.padding * 2.0f}};

	wp_clickable_state inputfield =
		button(file, line, input_aabb.pos, input_aabb.size, props, props.color,
			   props.border_width, false, false);

	if (wp_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT) && input->selected &&
		inputfield == WP_IDLE) {
		input->selected = false;
		state.input_grabbed = false;
		wp_input_field_unselect_all(input);
	} else if (inputfield == WP_CLICKED) {
		input->selected = true;
		state.input_grabbed = true;
		wp_text_props selected_props = wp_text_render(
			(vec2s){state.pos_ptr.x + props.padding,
					state.pos_ptr.y + props.padding},
			input->buf, font, WP_NO_COLOR, wrap_point,
			(vec2s){wp_get_mouse_x(), wp_get_mouse_y()}, true, false, -1, -1);
		input->cursor_index = selected_props.rendered_count;
	}

	if (input->selected) {
		char selected_buf[strlen(input->buf)];
		strncpy(selected_buf, input->buf, input->cursor_index);
		selected_buf[input->cursor_index] = '\0';

		wp_text_props selected_props =
			wp_text_render((vec2s){state.pos_ptr.x + props.padding,
								   wp_get_mouse_y() + props.padding},
						   selected_buf, font, WP_NO_COLOR, wrap_point,
						   (vec2s){-1, -1}, true, false, -1, -1);

		vec2s cursor_pos = {
			(strlen(input->buf) > 0) ? selected_props.end_x
									 : state.pos_ptr.x + props.padding,
			state.pos_ptr.y + props.padding +
				(selected_props.height - get_max_char_height_font(font))};
		if (input->selection_start == -1 || input->selection_end == -1) {
			wp_rect_render(cursor_pos,
						   (vec2s){1, get_max_char_height_font(font)},
						   props.text_color, WP_NO_COLOR, 0.0f, 0.0f);
		} else {
			wp_text_render((vec2s){state.pos_ptr.x + props.padding,
								   state.pos_ptr.y + props.padding},
						   input->buf, font, (wp_color){255, 255, 255, 80},
						   wrap_point, (vec2s){-1, -1}, false, true,
						   input->selection_start, input->selection_end);
		}
	}

	wp_text_render(
		(vec2s){state.pos_ptr.x + props.padding,
				state.pos_ptr.y + props.padding},
		(!strlen(input->buf) && !input->selected) ? input->placeholder
												  : input->buf,
		font,
		!strlen(input->buf) ? wp_color_brightness(props.text_color, 0.75f)
							: props.text_color,
		wrap_point, (vec2s){-1, -1}, false, false, -1, -1);

	state.pos_ptr.x += input->width + props.margin_right + props.padding * 2.0f;
	state.pos_ptr.y -= props.margin_top;
}

wp_font load_font(const char *filepath, uint32_t pixelsize, uint32_t tex_width,
				  uint32_t tex_height, uint32_t line_gap_add) {
	wp_font font = {0};
	/* Opening the file, reading the content to a buffer and parsing the loaded
	 * data with stb_truetype */
	FILE *file = fopen(filepath, "rb");
	if (file == NULL) {
		WP_ERROR("Failed to open font file '%s'\n", filepath);
	}

	// Loading the content
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	uint8_t *buffer = (uint8_t *)malloc(fileSize);
	size_t bytesRead = fread(buffer, 1, fileSize, file);
	fclose(file);
	if (bytesRead != fileSize) {
		WP_ERROR("Failed to read font file '%s'\n", filepath);
		// Handle the error (e.g., free memory and return from the function)
		free(buffer);
		wp_font emptyFont = {0}; // Or whatever initialization you need
		return emptyFont;
	}
	font.font_info = malloc(sizeof(stbtt_fontinfo));

	// Initializing the font with stb_truetype
	stbtt_InitFont((stbtt_fontinfo *)font.font_info, buffer,
				   stbtt_GetFontOffsetForIndex(buffer, 0));

	stbtt_fontinfo *fontinfo = (stbtt_fontinfo *)font.font_info;
	int numglyphs = fontinfo->numGlyphs;

	// Loading the font bitmap to memory by using stbtt_BakeFontBitmap
	uint8_t *bitmap =
		(uint8_t *)malloc(tex_width * tex_height * sizeof(uint32_t));
	uint8_t *bitmap_4bpp =
		(uint8_t *)malloc(tex_width * tex_height * 4 * sizeof(uint32_t));
	font.cdata = malloc(sizeof(stbtt_bakedchar) * numglyphs);
	font.tex_width = tex_width;
	font.tex_height = tex_height;
	font.line_gap_add = line_gap_add;
	font.font_size = pixelsize;
	font.num_glyphs = numglyphs;
	stbtt_BakeFontBitmap(buffer, 0, pixelsize, bitmap, tex_width, tex_height,
						 32, numglyphs, (stbtt_bakedchar *)font.cdata);

	uint32_t bitmap_index = 0;
	for (uint32_t i = 0; i < (uint32_t)(tex_width * tex_height * 4); i++) {
		bitmap_4bpp[i] = bitmap[bitmap_index];
		if ((i + 1) % 4 == 0) {
			bitmap_index++;
		}
	}

	// Creating an opengl texture (texture atlas) for the font
	glGenTextures(1, &font.texture.id);
	glBindTexture(GL_TEXTURE_2D, font.texture.id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					GL_LINEAR_MIPMAP_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_width, tex_height, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, bitmap_4bpp);
	glGenerateMipmap(GL_TEXTURE_2D);

	// Deallocating the bitmap data
	free(bitmap);
	free(bitmap_4bpp);
	return font;
}

wp_font get_current_font() {
	return state.font_stack ? *state.font_stack : state.theme.font;
}

wp_clickable_state button_element_loc(void *text, const char *file,
									  int32_t line, bool wide) {
	// Retrieving the property data of the button
	wp_element_props props = get_props_for(state.theme.button_props);
	float padding = props.padding;
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;

	// Advancing the position pointer by the margins
	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;
	wp_font font = get_current_font();

	wp_text_props text_props;
	if (wide)
		text_props = text_render_simple_wide(
			state.pos_ptr, (const wchar_t *)text, font, WP_NO_COLOR, true);
	else
		text_props = text_render_simple(state.pos_ptr, (const char *)text, font,
										WP_NO_COLOR, true);

	wp_color color = props.color;
	wp_color text_color =
		wp_hovered(state.pos_ptr,
				   (vec2s){text_props.width, text_props.height}) &&
				props.hover_text_color.a != 0.0f
			? props.hover_text_color
			: props.text_color;

	// If the button does not fit onto the current div, advance to the next line
	next_line_on_overflow(
		(vec2s){text_props.width + padding * 2.0f + margin_right + margin_left,
				text_props.height + padding * 2.0f + margin_bottom +
					margin_top},
		state.div_props.border_width);

	// Rendering the button
	wp_clickable_state ret =
		button(file, line, state.pos_ptr,
			   (vec2s){text_props.width + padding * 2,
					   text_props.height + padding * 2},
			   props, color, props.border_width, true, true);
	// Rendering the text of the button
	if (wide)
		text_render_simple_wide(
			(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding},
			(const wchar_t *)text, font, text_color, false);
	else
		text_render_simple(
			(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding},
			(const char *)text, font, text_color, false);

	// Advancing the position pointer by the width of the button
	state.pos_ptr.x += text_props.width + margin_right + padding * 2.0f;
	state.pos_ptr.y -= margin_top;

	return ret;
}
wp_clickable_state button_fixed_element_loc(void *text, float width,
											float height, const char *file,
											int32_t line, bool wide) {
	// Retrieving the property data of the button
	wp_element_props props = get_props_for(state.theme.button_props);
	float padding = props.padding;
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;

	wp_font font = state.font_stack ? *state.font_stack : state.theme.font;
	wp_text_props text_props;
	if (wide)
		text_props = text_render_simple_wide(
			state.pos_ptr, (const wchar_t *)text, font, WP_NO_COLOR, true);
	else
		text_props = text_render_simple(state.pos_ptr, (const char *)text, font,
										WP_NO_COLOR, true);

	wp_color color = props.color;
	wp_color text_color =
		wp_hovered(state.pos_ptr,
				   (vec2s){text_props.width, text_props.height}) &&
				props.hover_text_color.a != 0.0f
			? props.hover_text_color
			: props.text_color;

	// If the button does not fit onto the current div, advance to the next line
	int32_t render_width = ((width == -1) ? text_props.width : width);
	int32_t render_height = ((height == -1) ? text_props.height : height);

	next_line_on_overflow(
		(vec2s){render_width + padding * 2.0f + margin_right + margin_left,
				render_height + padding * 2.0f + margin_bottom + margin_top},
		state.div_props.border_width);

	// Advancing the position pointer by the margins
	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Rendering the button
	wp_clickable_state ret = button(
		file, line, state.pos_ptr,
		(vec2s){render_width + padding * 2.0f, render_height + padding * 2.0f},
		props, color, props.border_width, false, true);

	// Rendering the text of the button

	wp_set_cull_end_x(state.pos_ptr.x + render_width + padding);
	if (wide) {
		text_render_simple_wide(
			(vec2s){
				state.pos_ptr.x + padding +
					((width != -1) ? (width - text_props.width) / 2.0f : 0),
				state.pos_ptr.y + padding +
					((height != -1) ? (height - text_props.height) / 2.0f : 0)},
			(const wchar_t *)text, font, text_color, false);
	} else {
		text_render_simple(
			(vec2s){
				state.pos_ptr.x + padding +
					((width != -1) ? (width - text_props.width) / 2.0f : 0),
				state.pos_ptr.y + padding +
					((height != -1) ? (height - text_props.height) / 2.0f : 0)},
			(const char *)text, font, text_color, false);
	}
	wp_unset_cull_end_x();

	// Advancing the position pointer by the width of the button
	state.pos_ptr.x += render_width + margin_right + padding * 2.0f;
	state.pos_ptr.y -= margin_top;
	return ret;
}
wp_clickable_state checkbox_element_loc(void *text, bool *val,
										wp_color tick_color, wp_color tex_color,
										const char *file, int32_t line,
										bool wide) {
	// Retrieving the property values of the checkbox
	wp_font font = get_current_font();
	wp_element_props props = get_props_for(state.theme.checkbox_props);
	float margin_left = props.margin_left;
	float margin_right = props.margin_right;
	float margin_top = props.margin_top;
	float margin_bottom = props.margin_bottom;

	float checkbox_size;
	if (wide)
		checkbox_size = wp_text_dimension_wide((const wchar_t *)text).y;
	else
		checkbox_size = wp_text_dimension((const char *)text).y;

	// Advance to next line if the object does not fit on the div
	next_line_on_overflow(
		(vec2s){
			checkbox_size + margin_left + margin_right + props.padding * 2.0f,
			checkbox_size + margin_top + margin_bottom + props.padding * 2.0f},
		state.div_props.border_width);

	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Render the box
	wp_color checkbox_color =
		(*val) ? ((tick_color.a == 0) ? props.color : tick_color) : props.color;
	wp_clickable_state checkbox =
		button(file, line, state.pos_ptr,
			   (vec2s){checkbox_size + props.padding * 2.0f,
					   checkbox_size + props.padding * 2.0f},
			   props, checkbox_color, props.border_width, false, false);

	if (wide)
		text_render_simple_wide((vec2s){state.pos_ptr.x + checkbox_size +
											props.padding * 2.0f + margin_right,
										state.pos_ptr.y + props.padding},
								(const wchar_t *)text, font, props.text_color,
								false);
	else
		text_render_simple((vec2s){state.pos_ptr.x + checkbox_size +
									   props.padding * 2.0f + margin_right,
								   state.pos_ptr.y + props.padding},
						   (const char *)text, font, props.text_color, false);

	// Change the value if the checkbox is clicked
	if (checkbox == WP_CLICKED) {
		*val = !*val;
	}
	if (*val) {
		// Render the image
		wp_image_render((vec2s){state.pos_ptr.x + props.padding,
								state.pos_ptr.y + props.padding},
						tex_color,
						(wp_texture){.id = state.tex_tick.id,
									 .width = (uint32_t)(checkbox_size),
									 .height = (uint32_t)(checkbox_size)},
						(wp_color){0.0f, 0.0f, 0.0f, 0.0f}, 0,
						props.corner_radius);
	}
	state.pos_ptr.x += checkbox_size + props.padding * 2.0f + margin_right +
					   ((wide) ? wp_text_dimension_wide((const wchar_t *)text).x
							   : wp_text_dimension((const char *)text).x) +
					   margin_right;
	state.pos_ptr.y -= margin_top;

	return checkbox;
}
void dropdown_menu_item_loc(void **items, void *placeholder,
							uint32_t item_count, float width, float height,
							int32_t *selected_index, bool *opened,
							const char *file, int32_t line, bool wide) {
	wp_element_props props = get_props_for(state.theme.button_props);
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;
	float padding = props.padding;
	wp_font font = get_current_font();

	int32_t placeholder_strlen = (wide) ? wcslen((const wchar_t *)placeholder)
										: strlen((const char *)placeholder);
	void *button_text =
		(void *)((*selected_index != -1)	 ? items[*selected_index]
				 : (placeholder_strlen != 0) ? placeholder
											 : "Select");

	wp_text_props text_props;
	if (wide)
		text_props = text_render_simple_wide(
			(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding},
			(const wchar_t *)button_text, font, props.text_color, true);
	else
		text_props = text_render_simple(
			(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding},
			(const char *)button_text, font, props.text_color, true);

	float item_height =
		get_max_char_height_font(font) +
		((*opened) ? height + padding * 4.0f + margin_top : padding * 2.0f);
	next_line_on_overflow((vec2s){width + padding * 2.0f + margin_right,
								  item_height + margin_top + margin_bottom},
						  0.0f);

	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	vec2s button_pos = state.pos_ptr;
	wp_clickable_state dropdown_button =
		button(file, line, state.pos_ptr,
			   (vec2s){(float)width + padding * 2.0f,
					   (float)text_props.height + padding * 2.0f},
			   props, props.color, props.border_width, false, true);

	if (wide)
		text_render_simple_wide(
			(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding},
			(const wchar_t *)button_text, font, props.text_color, false);
	else
		text_render_simple(
			(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding},
			(const char *)button_text, font, props.text_color, false);

	// Render dropdown arrow
	{
		vec2s image_size = (vec2s){20, 10};
		wp_image_render(
			(vec2s){state.pos_ptr.x + width + padding - image_size.x,
					state.pos_ptr.y +
						((text_props.height + padding * 2) - image_size.y) /
							2.0f},
			props.text_color,
			(wp_texture){.id = state.tex_arrow_down.id,
						 .width = (uint32_t)image_size.x,
						 .height = (uint32_t)image_size.y},
			WP_NO_COLOR, 0.0f, 0.0f);
	}

	if (dropdown_button == WP_CLICKED) {
		*opened = !*opened;
	}

	if (*opened) {
		if ((wp_mouse_button_up(GLFW_MOUSE_BUTTON_LEFT) &&
			 dropdown_button != WP_CLICKED) ||
			(!wp_input_grabbed() && wp_key_down(GLFW_KEY_ESCAPE))) {
			*opened = false;
		}

		wp_element_props div_props = wp_get_theme().div_props;
		div_props.corner_radius = props.corner_radius;
		div_props.border_color = props.border_color;
		div_props.border_width = props.border_width;
		div_props.color = props.color;
		wp_push_style_props(div_props);
		wp_div_begin(
			((vec2s){state.pos_ptr.x,
					 state.pos_ptr.y + text_props.height + padding * 2.0f}),
			((vec2s){width + props.padding * 2.0f,
					 height + props.padding * 2.0f}),
			false);
		wp_pop_style_props();

		for (uint32_t i = 0; i < item_count; i++) {
			wp_element_props text_props = wp_get_theme().text_props;
			text_props.text_color = props.text_color;
			bool hovered =
				wp_hovered((vec2s){state.pos_ptr.x + text_props.margin_left,
								   state.pos_ptr.y + text_props.margin_top},
						   (vec2s){width + props.padding * 2.0f,
								   wp_get_theme().font.font_size});
			if (hovered) {
				wp_rect_render(
					((vec2s){state.pos_ptr.x, state.pos_ptr.y}),
					(vec2s){width + props.padding * 2.0f,
							wp_get_theme().font.font_size + props.margin_top},
					wp_color_brightness(div_props.color, 1.2f), WP_NO_COLOR,
					0.0f, 0.0f);
			}

			if (hovered && wp_mouse_button_up(GLFW_MOUSE_BUTTON_LEFT)) {
				*selected_index = i;
			}
			wp_push_style_props(text_props);
			wp_text(items[i]);
			wp_pop_style_props();
			wp_next_line();
		}
		wp_div_end();
	}

	state.pos_ptr.x += width + padding * 2.0f + margin_right;
	state.pos_ptr.y -= margin_top;

	wp_push_style_props(props);
}
int32_t menu_item_list_item_loc(void **items, uint32_t item_count,
								int32_t selected_index,
								wp_menu_item_callback per_cb, bool vertical,
								const char *file, int32_t line, bool wide) {
	wp_element_props props = get_props_for(state.theme.button_props);
	float padding = props.padding;
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;
	wp_color color = props.color;
	wp_font font = get_current_font();

	wp_text_props text_props[item_count];
	float width = 0;
	for (uint32_t i = 0; i < item_count; i++) {
		if (wide)
			text_props[i] = text_render_simple_wide(
				(vec2s){state.pos_ptr.x, state.pos_ptr.y + margin_top},
				(const wchar_t *)items[i], font, props.text_color, true);
		else
			text_props[i] = text_render_simple(
				(vec2s){state.pos_ptr.x, state.pos_ptr.y + margin_top},
				(const char *)items[i], font, props.text_color, true);
		width += text_props[i].width + padding * 2.0f;
	}
	next_line_on_overflow(
		(vec2s){width + padding * 2.0f + margin_right + margin_left,
				font.font_size + padding * 2.0f + margin_bottom + margin_top},
		state.div_props.border_width);

	state.pos_ptr.y += margin_top;
	state.pos_ptr.x += margin_left;

	uint32_t element_width = 0;
	uint32_t clicked_item = -1;
	for (uint32_t i = 0; i < item_count; i++) {
		wp_element_props props = state.theme.button_props;
		props.margin_left = 0;
		props.margin_right = 0;
		wp_element_props button_props = state.theme.button_props;
		wp_push_style_props(props);
		if (i == selected_index) {
			props.color = wp_color_brightness(props.color, 1.2);
		}
		wp_push_style_props(props);
		if (wide) {
			if (_wp_button_wide_loc((const wchar_t *)items[i], file, line) ==
				WP_CLICKED) {
				clicked_item = i;
			}
		} else {
			if (_wp_button_loc((const char *)items[i], file, line) ==
				WP_CLICKED) {
				clicked_item = i;
			}
		}
		wp_pop_style_props();
		per_cb(&i);
	}
	next_line_on_overflow((vec2s){element_width + margin_right,
								  font.font_size + margin_top + margin_bottom},
						  state.div_props.border_width);

	state.pos_ptr.y -= margin_top;
	return clicked_item;
}

static int32_t get_max_char_height_font(wp_font font) {
	float fontScale = stbtt_ScaleForPixelHeight(
		(stbtt_fontinfo *)font.font_info, font.font_size);
	int32_t xmin, ymin, xmax, ymax;
	int32_t codepoint = 'p';
	stbtt_GetCodepointBitmapBox((stbtt_fontinfo *)font.font_info, codepoint,
								fontScale, fontScale, &xmin, &ymin, &xmax,
								&ymax);
	return ymax - ymin;
}
void remove_i_str(char *str, int32_t index) {
	int32_t len = strlen(str);
	if (index >= 0 && index < len) {
		for (int32_t i = index; i < len - 1; i++) {
			str[i] = str[i + 1];
		}
		str[len - 1] = '\0';
	}
}

void remove_substr_str(char *str, int start_index, int end_index) {
	int len = strlen(str);

	memmove(str + start_index, str + end_index + 1, len - end_index);
	str[len - (end_index - start_index) + 1] = '\0';
}

void insert_i_str(char *str, char ch, int32_t index) {
	int len = strlen(str);

	if (index < 0 || index > len) {
		WP_ERROR("Invalid string index for inserting.\n");
		return;
	}

	for (int i = len; i > index; i--) {
		str[i] = str[i - 1];
	}

	str[index] = ch;
	str[len + 1] = '\0';
}

void insert_str_str(char *source, const char *insert, int32_t index) {
	int source_len = strlen(source);
	int insert_len = strlen(insert);

	if (index < 0 || index > source_len) {
		WP_ERROR("Index for inserting out of bounds\n");
		return;
	}

	memmove(source + index + insert_len, source + index,
			source_len - index + 1);

	memcpy(source + index, insert, insert_len);
}

void substr_str(const char *str, int start_index, int end_index,
				char *substring) {
	int substring_length = end_index - start_index + 1;
	strncpy(substring, str + start_index, substring_length);
	substring[substring_length] = '\0';
}

int map_vals(int value, int from_min, int from_max, int to_min, int to_max) {
	return (value - from_min) * (to_max - to_min) / (from_max - from_min) +
		   to_min;
}

void glfw_key_callback(GLFWwindow *window, int32_t key, int scancode,
					   int action, int mods) {
	(void)window;
	(void)mods;
	(void)scancode;
	// Changing the the keys array to resamble the state of the keyboard
	if (action != GLFW_RELEASE) {
		if (!state.input.keyboard.keys[key])
			state.input.keyboard.keys[key] = true;
	} else {
		state.input.keyboard.keys[key] = false;
	}
	state.input.keyboard.keys_changed[key] = (action != GLFW_REPEAT);

	// Calling user defined callbacks
	for (uint32_t i = 0; i < state.input.key_cb_count; i++) {
		state.input.key_cbs[i](window, key, scancode, action, mods);
	}

	// Populating the key event
	state.key_ev.happened = true;
	state.key_ev.pressed = action != GLFW_RELEASE;
	state.key_ev.keycode = key;
}
void glfw_mouse_button_callback(GLFWwindow *window, int32_t button, int action,
								int mods) {
	(void)window;
	(void)mods;
	// Changing the buttons array to resamble the state of the mouse
	if (action != GLFW_RELEASE) {
		if (!state.input.mouse.buttons_current[button])
			state.input.mouse.buttons_current[button] = true;
	} else {
		state.input.mouse.buttons_current[button] = false;
	}
	// Calling user defined callbacks
	for (uint32_t i = 0; i < state.input.mouse_button_cb_count; i++) {
		state.input.mouse_button_cbs[i](window, button, action, mods);
	}
	// Populating the mouse button event
	state.mb_ev.happened = true;
	state.mb_ev.pressed = action != GLFW_RELEASE;
	state.mb_ev.button_code = button;
}
void glfw_scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
	(void)window;
	// Setting the scroll values
	state.input.mouse.xscroll_delta = xoffset;
	state.input.mouse.yscroll_delta = yoffset;

	// Calling user defined callbacks
	for (uint32_t i = 0; i < state.input.scroll_cb_count; i++) {
		state.input.scroll_cbs[i](window, xoffset, yoffset);
	}
	// Populating the scroll event
	state.scr_ev.happened = true;
	state.scr_ev.x_off = xoffset;
	state.scr_ev.y_off = yoffset;

	// Scrolling the current div
	wp_div *selected_div = &state.selected_div;
	if (!selected_div->scrollable)
		return;
	if ((state.grabbed_div.id != -1 &&
		 selected_div->id != state.grabbed_div.id))
		return;

	if (yoffset < 0.0f) {
		if (selected_div->total_area.y >
			(selected_div->aabb.size.y + selected_div->aabb.pos.y)) {
			if (state.theme.div_smooth_scroll) {
				*state.scroll_velocity_ptr -=
					state.theme.div_scroll_acceleration;
				state.div_velocity_accelerating = true;
			} else {
				*state.scroll_ptr -= state.theme.div_scroll_amount_px;
			}
		}
	} else if (yoffset > 0.0f) {
		if (*state.scroll_ptr) {
			if (state.theme.div_smooth_scroll) {
				*state.scroll_velocity_ptr +=
					state.theme.div_scroll_acceleration;
				state.div_velocity_accelerating = false;
			} else {
				*state.scroll_ptr += state.theme.div_scroll_amount_px;
			}
		}
	}
	if (state.theme.div_smooth_scroll)
		*state.scroll_velocity_ptr =
			MIN(MAX(*state.scroll_velocity_ptr,
					-state.theme.div_scroll_max_velocity),
				state.theme.div_scroll_max_velocity);
}
void glfw_cursor_callback(GLFWwindow *window, double xpos, double ypos) {
	(void)window;
	mouse_t *mouse = &state.input.mouse;
	// Setting the position values
	mouse->xpos = xpos;
	mouse->ypos = ypos;

	if (mouse->first_mouse_press) {
		mouse->xpos_last = xpos;
		mouse->ypos_last = ypos;
		mouse->first_mouse_press = false;
	}
	// Delta mouse positions
	mouse->xpos_delta = mouse->xpos - mouse->xpos_last;
	mouse->ypos_delta = mouse->ypos - mouse->ypos_last;
	mouse->xpos_last = xpos;
	mouse->ypos_last = ypos;

	// Calling User defined callbacks
	for (uint32_t i = 0; i < state.input.cursor_pos_cb_count; i++) {
		state.input.cursor_pos_cbs[i](window, xpos, ypos);
	}

	// Populating the cursor event
	state.cp_ev.happened = true;
	state.cp_ev.x = xpos;
	state.cp_ev.y = ypos;
}
void glfw_char_callback(GLFWwindow *window, uint32_t charcode) {
	(void)window;
	state.ch_ev.charcode = charcode;
	state.ch_ev.happened = true;
}

void update_input() {
	memcpy(state.input.mouse.buttons_last, state.input.mouse.buttons_current,
		   sizeof(bool) * MAX_MOUSE_BUTTONS);
}

void clear_events() {
	state.key_ev.happened = false;
	state.mb_ev.happened = false;
	state.cp_ev.happened = false;
	state.scr_ev.happened = false;
	state.ch_ev.happened = false;
	state.input.mouse.xpos_delta = 0;
	state.input.mouse.ypos_delta = 0;
}

uint64_t djb2_hash(uint64_t hash, const void *buf, size_t size) {
	uint8_t *bytes = (uint8_t *)buf;
	int c;

	while ((c = *bytes++)) {
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

void props_stack_create(props_stack_t *stack) {
	stack->data = (wp_element_props *)malloc(WP_STACK_INIT_CAP *
											 sizeof(wp_element_props));
	if (!stack->data) {
		WP_ERROR("Failed to allocate memory for stack data structure.\n");
	}
	stack->count = 0;
	stack->cap = WP_STACK_INIT_CAP;
}

void props_stack_resize(props_stack_t *stack, uint32_t newcap) {
	wp_element_props *newdata = (wp_element_props *)realloc(
		stack->data, newcap * sizeof(wp_element_props));
	if (!newdata) {
		WP_ERROR("Failed to reallocate memory for stack datastructure.");
	}
	stack->data = newdata;
	stack->cap = newcap;
}

void props_stack_push(props_stack_t *stack, wp_element_props props) {
	if (stack->count == stack->cap) {
		props_stack_resize(stack, stack->cap * 2);
	}
	stack->data[stack->count++] = props;
}

wp_element_props props_stack_pop(props_stack_t *stack) {
	WP_ASSERT(stack.count != 0, "Stack underflow on stack data structure!");
	wp_element_props val = stack->data[--stack->count];
	if (stack->count > 0 && stack->count == stack->cap / 4) {
		props_stack_resize(stack, stack->cap / 2);
	}
	return val;
}

wp_element_props props_stack_peak(props_stack_t *stack) {
	WP_ASSERT(stack.count != 0, "Stack is empty on stack data structure!");
	return stack->data[stack->count - 1];
}

bool props_stack_empty(props_stack_t *stack) { return stack->count == 0; }

wp_element_props get_props_for(wp_element_props props) {
	return (!props_stack_empty(&state.props_stack))
			   ? props_stack_peak(&state.props_stack)
			   : props;
}

// ===========================================================
// ----------------Public API Functions ----------------------
// ===========================================================
void wp_init_glfw(uint32_t display_width, uint32_t display_height,
				  void *glfw_window) {
	setlocale(LC_ALL, "");
	if (!glfwInit()) {
		WP_ERROR("Trying to initialize Leif with GLFW without initializing "
				 "GLFW first.");
		return;
	}

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		WP_ERROR("Failed to initialize Glad.");
		return;
	}
	memset(&state, 0, sizeof(state));

	// Default state
	state.init = true;
	state.dsp_w = display_width;
	state.dsp_h = display_height;
	state.window_handle = glfw_window;
	state.input.mouse.first_mouse_press = true;
	state.render.tex_count = 0;
	state.pos_ptr = (vec2s){0, 0};
	state.image_color_stack = WP_NO_COLOR;
	state.active_element_id = 0;
	state.text_wrap = false;
	state.line_overflow = true;
	state.theme = wp_default_theme();
	state.renderer_render = true;
	state.drag_state = (wp_drag_state){false, {0, 0}, 0};

	props_stack_create(&state.props_stack);

	memset(&state.grabbed_div, 0, sizeof(wp_div));
	state.grabbed_div.id = -1;

	state.clipboard = clipboard_new(NULL);

	state.drawcalls = 0;

	// Setting glfw callbacks
	glfwSetKeyCallback((GLFWwindow *)state.window_handle, glfw_key_callback);
	glfwSetMouseButtonCallback((GLFWwindow *)state.window_handle,
							   glfw_mouse_button_callback);
	glfwSetScrollCallback((GLFWwindow *)state.window_handle,
						  glfw_scroll_callback);
	glfwSetCursorPosCallback((GLFWwindow *)state.window_handle,
							 glfw_cursor_callback);
	glfwSetCharCallback((GLFWwindow *)state.window_handle, glfw_char_callback);
	renderer_init();

	state.tex_arrow_down = wp_load_texture_asset("arrow-down", "png");
	state.tex_tick = wp_load_texture_asset("tick", "png");
}

void wp_terminate() { wp_free_font(&state.theme.font); }

wp_theme wp_default_theme() {
	// The default theme of Leif
	wp_theme theme = {0};
	theme.div_props = (wp_element_props){
		.color = (wp_color){45, 45, 45, 255},
		.border_color = (wp_color){0, 0, 0, 0},
		.border_width = 0.0f,
		.corner_radius = 0.0f,
		.hover_color = WP_NO_COLOR,
	};
	float global_padding = 10;
	float global_margin = 5;
	theme.text_props = (wp_element_props){.text_color = WP_WHITE,
										  .border_color = WP_NO_COLOR,
										  .padding = 0,
										  .margin_left = global_margin,
										  .margin_right = global_margin,
										  .margin_top = global_margin,
										  .margin_bottom = global_margin,
										  .border_width = global_margin,
										  .corner_radius = 0,
										  .hover_color = WP_NO_COLOR,
										  .hover_text_color = WP_NO_COLOR};
	theme.button_props =
		(wp_element_props){.color = WP_PRIMARY_ITEM_COLOR,
						   .text_color = WP_BLACK,
						   .border_color = WP_SECONDARY_ITEM_COLOR,
						   .padding = global_padding,
						   .margin_left = global_margin,
						   .margin_right = global_margin,
						   .margin_top = global_margin,
						   .margin_bottom = global_margin,
						   .border_width = 4,
						   .corner_radius = 0,
						   .hover_color = WP_NO_COLOR,
						   .hover_text_color = WP_NO_COLOR};
	theme.image_props = (wp_element_props){.color = WP_WHITE,
										   .margin_left = global_margin,
										   .margin_right = global_margin,
										   .margin_top = global_margin,
										   .margin_bottom = global_margin,
										   .corner_radius = 0,
										   .hover_color = WP_NO_COLOR,
										   .hover_text_color = WP_NO_COLOR};
	theme.inputfield_props =
		(wp_element_props){.color = WP_PRIMARY_ITEM_COLOR,
						   .text_color = WP_BLACK,
						   .border_color = WP_SECONDARY_ITEM_COLOR,
						   .padding = global_padding,
						   .margin_left = global_margin,
						   .margin_right = global_margin,
						   .margin_top = global_margin,
						   .margin_bottom = global_margin,
						   .border_width = 4,
						   .corner_radius = 0,
						   .hover_color = WP_NO_COLOR,
						   .hover_text_color = WP_NO_COLOR};
	theme.checkbox_props =
		(wp_element_props){.color = WP_PRIMARY_ITEM_COLOR,
						   .text_color = WP_WHITE,
						   .border_color = WP_SECONDARY_ITEM_COLOR,
						   .padding = global_padding,
						   .margin_left = global_margin,
						   .margin_right = global_margin,
						   .margin_top = global_margin,
						   .margin_bottom = global_margin,
						   .border_width = 4,
						   .corner_radius = 0,
						   .hover_color = WP_NO_COLOR,
						   .hover_text_color = WP_NO_COLOR};
	theme.slider_props =
		(wp_element_props){.color = WP_PRIMARY_ITEM_COLOR,
						   .text_color = WP_SECONDARY_ITEM_COLOR,
						   .border_color = WP_SECONDARY_ITEM_COLOR,
						   .padding = global_padding,
						   .margin_left = global_margin,
						   .margin_right = global_margin,
						   .margin_top = global_margin,
						   .margin_bottom = global_margin,
						   .border_width = 4,
						   .corner_radius = 0,
						   .hover_color = WP_NO_COLOR,
						   .hover_text_color = WP_NO_COLOR};
	theme.scrollbar_props = (wp_element_props){.color = WP_SECONDARY_ITEM_COLOR,
											   .border_color = WP_BLACK,
											   .padding = 0,
											   .margin_left = 0,
											   .margin_right = 5,
											   .margin_top = 5,
											   .margin_bottom = 0,
											   .border_width = 0,
											   .corner_radius = 0,
											   .hover_color = WP_NO_COLOR,
											   .hover_text_color = WP_NO_COLOR};
	theme.font = wp_load_font_asset("inter", "ttf", 24);

	theme.div_scroll_max_velocity = 100.0f;
	theme.div_scroll_velocity_deceleration = 0.92;
	theme.div_scroll_acceleration = 2.5f;
	theme.div_scroll_amount_px = 20.0f;
	theme.div_smooth_scroll = true;

	theme.scrollbar_width = 8;

	return theme;
}

wp_theme wp_get_theme() { return state.theme; }

void wp_set_theme(wp_theme theme) { state.theme = theme; }

void wp_resize_display(uint32_t display_width, uint32_t display_height) {
	// Setting the display height internally
	state.dsp_w = display_width;
	state.dsp_h = display_height;

	set_projection_matrix();

	state.current_div.aabb.size.x = state.dsp_w;
	state.current_div.aabb.size.y = state.dsp_h;
}

wp_font wp_load_font(const char *filepath, uint32_t size) {
	return load_font(filepath, size, 1024, 1024, 0);
}

wp_font wp_load_font_ex(const char *filepath, uint32_t size, uint32_t bitmap_w,
						uint32_t bitmap_h) {
	return load_font(filepath, size, bitmap_w, bitmap_h, 0);
}

wp_texture wp_load_texture(const char *filepath, bool flip,
						   wp_texture_filtering filter) {
	wp_texture tex;
	int width, height, channels;
	unsigned char *image =
		stbi_load(filepath, &width, &height, &channels, STBI_rgb_alpha);
	if (!image) {
		WP_ERROR("Failed to load texture at '%s'.", filepath);
		return tex;
	}

	glGenTextures(1, &tex.id);
	glBindTexture(GL_TEXTURE_2D, tex.id);

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	switch (filter) {
	case WP_LINEAR:
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER,
							GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER,
							GL_LINEAR_MIPMAP_LINEAR);
		break;
	case WP_NEAREST:
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		break;
	}

	// Load texture data
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(image); // Free image data
	//
	tex.width = width;
	tex.height = height;
	return tex;
}

wp_texture wp_load_texture_resized(const char *filepath, bool flip,
								   wp_texture_filtering filter, uint32_t w,
								   uint32_t h) {
	wp_texture tex;
	int32_t width, height, channels;
	stbi_uc *image_data = stbi_load(filepath, &width, &height, &channels, 0);

	unsigned char *downscaled_image =
		(unsigned char *)malloc(sizeof(unsigned char) * w * h * channels);

	// Resize the original image to the downscaled size
	stbir_resize_uint8_linear(image_data, width, height, 0, downscaled_image, w,
							  h, 0, (stbir_pixel_layout)channels);

	glCreateTextures(GL_TEXTURE_2D, 1, &tex.id);
	glBindTexture(GL_TEXTURE_2D, tex.id);

	switch (filter) {
	case WP_LINEAR:
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		break;
	case WP_NEAREST:
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		break;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE,
				 downscaled_image);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(image_data);
	free(downscaled_image);

	tex.width = width;
	tex.height = height;

	return tex;
}

wp_texture wp_load_texture_resized_factor(const char *filepath, bool flip,
										  wp_texture_filtering filter,
										  float wfactor, float hfactor) {
	// Loading the texture into memory with stb_image
	wp_texture tex;
	int32_t width, height, channels;
	stbi_uc *data = wp_load_texture_data_resized_factor(
		filepath, wfactor, hfactor, &width, &height, &channels, flip);

	if (!data) {
		WP_ERROR("Failed to load texture file at '%s'.", filepath);
		return tex;
	}

	int32_t w = width * wfactor;
	int32_t h = height * hfactor;
	wp_create_texture_from_image_data(data, &tex.id, w, h, channels, filter);

	free(data);

	tex.width = w;
	tex.height = h;

	return tex;
}

wp_texture wp_load_texture_from_memory(const void *data, size_t size, bool flip,
									   wp_texture_filtering filter) {
	wp_texture tex;
	int width, height, channels;
	unsigned char *image = stbi_load_from_memory(data, size, &width, &height,
												 &channels, STBI_rgb_alpha);
	if (!image) {
		return tex;
	}

	glGenTextures(1, &tex.id);
	glBindTexture(GL_TEXTURE_2D, tex.id);

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	switch (filter) {
	case WP_LINEAR:
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER,
							GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER,
							GL_LINEAR_MIPMAP_LINEAR);
		break;
	case WP_NEAREST:
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		break;
	}

	// Load texture data
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(image); // Free image data
	tex.width = width;
	tex.height = height;
	return tex;
}
wp_texture wp_load_texture_from_memory_resized(const void *data, size_t size,
											   bool flip,
											   wp_texture_filtering filter,
											   uint32_t w, uint32_t h) {
	wp_texture tex;

	int32_t channels;
	unsigned char *resized = wp_load_texture_data_from_memory_resized(
		data, size, &channels, NULL, NULL, flip, w, h);
	wp_create_texture_from_image_data(resized, &tex.id, w, h, channels,
									  WP_LINEAR);

	tex.width = w;
	tex.height = h;

	return tex;
}

wp_texture wp_load_texture_from_memory_resized_factor(
	const void *data, size_t size, bool flip, wp_texture_filtering filter,
	float wfactor, float hfactor) {
	wp_texture tex;

	int32_t width, height, channels;
	stbi_uc *image_data = stbi_load_from_memory((const stbi_uc *)data, size,
												&width, &height, &channels, 0);

	int w = width * wfactor;
	int h = height * hfactor;

	unsigned char *resized_data =
		(unsigned char *)malloc(sizeof(unsigned char) * w * h * channels);
	stbir_resize_uint8_linear(image_data, width, height, 0, resized_data, w, h,
							  0, (stbir_pixel_layout)channels);
	stbi_image_free(image_data);

	wp_create_texture_from_image_data(resized_data, &tex.id, w, h, channels,
									  WP_LINEAR);

	tex.width = w;
	tex.height = h;

	return tex;
}

wp_texture wp_load_texture_from_memory_resized_to_fit(
	const void *data, size_t size, bool flip, wp_texture_filtering filter,
	int32_t container_w, int32_t container_h) {
	wp_texture tex;

	int32_t image_width, image_height, channels;
	stbi_uc *image_data = wp_load_texture_data_from_memory(
		(const stbi_uc *)data, size, &image_width, &image_height, &channels,
		flip);

	int32_t new_width, new_height;
	unsigned char *resized_data =
		wp_load_texture_data_from_memory_resized_to_fit_ex(
			image_data, size, &new_width, &new_height, channels, image_width,
			image_height, flip, container_w, container_h);
	stbi_image_free(image_data);

	wp_create_texture_from_image_data(resized_data, &tex.id, new_width,
									  new_height, channels, WP_LINEAR);

	tex.width = new_width;
	tex.height = new_height;

	return tex;
}
unsigned char *wp_load_texture_data(const char *filepath, int32_t *width,
									int32_t *height, int32_t *channels,
									bool flip) {
	stbi_set_flip_vertically_on_load(!flip);
	stbi_uc *data =
		stbi_load(filepath, width, height, channels, STBI_rgb_alpha);
	return data;
}

unsigned char *wp_load_texture_data_resized(const char *filepath, int32_t w,
											int32_t h, int32_t *channels,
											bool flip) {
	int32_t width, height;
	stbi_uc *data =
		wp_load_texture_data(filepath, &width, &height, channels, flip);
	unsigned char *downscaled_image =
		(unsigned char *)malloc(sizeof(unsigned char) * w * h * *channels);
	stbir_resize_uint8_linear(data, width, height, *channels, downscaled_image,
							  w, h, 0, (stbir_pixel_layout)*channels);
	stbi_image_free(data);
	return downscaled_image;
}

unsigned char *wp_load_texture_data_resized_factor(
	const char *filepath, int32_t wfactor, int32_t hfactor, int32_t *width,
	int32_t *height, int32_t *channels, bool flip) {
	unsigned char *image =
		stbi_load(filepath, width, height, channels, STBI_rgb_alpha);
	if (!image) {
		return NULL;
	}

	float w = (wfactor * (*width));
	float h = (hfactor * (*height));

	size_t new_size = w * h * (*channels);
	unsigned char *resized_data = (unsigned char *)malloc(new_size);
	if (resized_data == NULL) {
		return NULL;
	}

	stbir_resize_uint8_linear(image, *width, *height, *channels, resized_data,
							  w, h, 0, (stbir_pixel_layout)*channels);
	free(image);
	return resized_data;
}

unsigned char *wp_load_texture_data_from_memory(const void *data, size_t size,
												int32_t *width, int32_t *height,
												int32_t *channels, bool flip) {
	stbi_set_flip_vertically_on_load(!flip);
	unsigned char *image =
		stbi_load_from_memory(data, size, width, height, channels, 0);
	if (!image) {
		return NULL;
	}
	return image;
}

unsigned char *wp_load_texture_data_from_memory_resized(
	const void *data, size_t size, int32_t *channels, int32_t *o_w,
	int32_t *o_h, bool flip, uint32_t w, uint32_t h) {
	int32_t width, height;
	stbi_uc *image_data = stbi_load_from_memory((const stbi_uc *)data, size,
												&width, &height, channels, 0);
	unsigned char *resized_data =
		wp_load_texture_data_from_memory_resized_to_fit_ex(
			image_data, size, o_w, o_h, *channels, width, height, flip, 48, 48);
	stbi_image_free(image_data);
	return resized_data;
}

unsigned char *wp_load_texture_data_from_memory_resized_to_fit_ex(
	const void *data, size_t size, int32_t *o_width, int32_t *o_height,
	int32_t i_channels, int32_t i_width, int32_t i_height, bool flip,
	int32_t container_w, int32_t container_h) {
	float container_aspect_ratio = (float)container_w / container_h;
	float image_aspect_ratio = (float)i_width / i_height;

	int new_width, new_height;

	if (image_aspect_ratio > container_aspect_ratio) {
		new_width = container_w;
		new_height = (int)((container_w / (float)i_width) * i_height);
	} else {
		new_height = container_h;
		new_width = (int)((container_h / (float)i_height) * i_width);
	}

	if (o_width)
		*o_width = new_width;
	if (o_height)
		*o_height = new_height;

	unsigned char *resized_image = (unsigned char *)malloc(
		sizeof(unsigned char) * new_width * new_height * i_channels);
	stbir_resize_uint8_linear(data, i_width, i_height, 0, resized_image,
							  new_width, new_height, 0,
							  (stbir_pixel_layout)i_channels);
	return resized_image;
}
unsigned char *wp_load_texture_data_from_memory_resized_to_fit(
	const void *data, size_t size, int32_t *o_width, int32_t *o_height,
	int32_t *o_channels, bool flip, int32_t container_w, int32_t container_h) {

	int32_t image_width, image_height, channels;
	stbi_uc *image_data = wp_load_texture_data_from_memory(
		(const stbi_uc *)data, size, &image_width, &image_height, &channels,
		flip);

	int32_t new_width, new_height;
	unsigned char *resized_data =
		wp_load_texture_data_from_memory_resized_to_fit_ex(
			image_data, size, &new_width, &new_height, channels, image_width,
			image_height, flip, container_w, container_h);
	*o_width = new_width;
	*o_height = new_height;
	*o_channels = channels;

	stbi_image_free(image_data);
	return image_data;
}

unsigned char *wp_load_texture_data_from_memory_resized_factor(
	const void *data, size_t size, int32_t *width, int32_t *height,
	int32_t *channels, bool flip, float wfactor, float hfactor) {
	stbi_uc *image_data = stbi_load_from_memory((const stbi_uc *)data, size,
												width, height, channels, 0);

	int w = (*width) * wfactor;
	int h = (*height) * hfactor;

	unsigned char *resized_data =
		(unsigned char *)malloc(sizeof(unsigned char) * w * h * (*channels));
	stbir_resize_uint8_linear(image_data, *width, *height, 0, resized_data, w,
							  h, 0, (stbir_pixel_layout)*channels);
	stbi_image_free(image_data);
	return resized_data;
}

void wp_create_texture_from_image_data(unsigned char *data, uint32_t *id,
									   int32_t width, int32_t height,
									   int32_t channels,
									   wp_texture_filtering filter) {
	GLenum internal_format = (channels == 4) ? GL_RGBA8 : GL_RGB8;
	GLenum data_format = (channels == 4) ? GL_RGBA : GL_RGB;
	glGenTextures(1, id);
	glBindTexture(GL_TEXTURE_2D, *id);

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	switch (filter) {
	case WP_LINEAR:
		glTextureParameteri(*id, GL_TEXTURE_MIN_FILTER,
							GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(*id, GL_TEXTURE_MAG_FILTER,
							GL_LINEAR_MIPMAP_LINEAR);
		break;
	case WP_NEAREST:
		glTextureParameteri(*id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(*id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		break;
	}

	// Load texture data
	glTexImage2D(GL_TEXTURE_2D, 0, data_format, width, height, 0, data_format,
				 GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
}

void wp_free_texture(wp_texture *tex) {
	glDeleteTextures(1, &tex->id);
	memset(tex, 0, sizeof(wp_texture));
}

void wp_free_font(wp_font *font) {
	free(font->cdata);
	free(font->font_info);
}

wp_font wp_load_font_asset(const char *asset_name, const char *file_extension,
						   uint32_t font_size) {
	char leif_dir[strlen(getenv(HOMEDIR)) + strlen("/.leif")];
	memset(leif_dir, 0, sizeof(leif_dir));
	strcat(leif_dir, getenv(HOMEDIR));
	strcat(leif_dir, "/.leif");

	char path[strlen(leif_dir) + strlen("/assets/fonts/") + strlen(asset_name) +
			  strlen(".") + strlen(file_extension)];

	memset(path, 0, sizeof(path));
	strcat(path, leif_dir);
	strcat(path, "/assets/fonts/");
	strcat(path, asset_name);
	strcat(path, ".");
	strcat(path, file_extension);

	return wp_load_font(path, font_size);
}

wp_texture wp_load_texture_asset(const char *asset_name,
								 const char *file_extension) {
	char leif_dir[strlen(getenv(HOMEDIR)) + strlen("/.leif")];
	memset(leif_dir, 0, sizeof(leif_dir));
	strcat(leif_dir, getenv(HOMEDIR));
	strcat(leif_dir, "/.leif");

	char path[strlen(leif_dir) + strlen("/assets/textures/") +
			  strlen(asset_name) + strlen(".") + strlen(file_extension)];
	memset(path, 0, sizeof(path));
	strcat(path, leif_dir);
	strcat(path, "/assets/textures/");
	strcat(path, asset_name);
	strcat(path, ".");
	strcat(path, file_extension);

	return wp_load_texture(path, false, WP_LINEAR);
}

void wp_add_key_callback(void *cb) {
	state.input.key_cbs[state.input.key_cb_count++] = (KEY_CALLBACK_t)cb;
}
void wp_add_mouse_button_callback(void *cb) {
	state.input.mouse_button_cbs[state.input.mouse_button_cb_count++] =
		(MOUSE_BUTTON_CALLBACK_t)cb;
}

void wp_add_scroll_callback(void *cb) {
	state.input.scroll_cbs[state.input.scroll_cb_count++] =
		(SCROLL_CALLBACK_t)cb;
}

void wp_add_cursor_pos_callback(void *cb) {
	state.input.cursor_pos_cbs[state.input.cursor_pos_cb_count++] =
		(CURSOR_CALLBACK_t)cb;
}

bool wp_key_down(uint32_t key) {
	return wp_key_changed(key) && state.input.keyboard.keys[key];
}

bool wp_key_held(uint32_t key) { return state.input.keyboard.keys[key]; }

bool wp_key_up(uint32_t key) {
	return wp_key_changed(key) && !state.input.keyboard.keys[key];
}

bool wp_key_changed(uint32_t key) {
	bool ret = state.input.keyboard.keys_changed[key];
	state.input.keyboard.keys_changed[key] = false;
	return ret;
}

bool wp_mouse_button_down(uint32_t button) {
	return wp_mouse_button_changed(button) &&
		   state.input.mouse.buttons_current[button];
}

bool wp_mouse_button_held(uint32_t button) {
	return state.input.mouse.buttons_current[button];
}

bool wp_mouse_button_up(uint32_t button) {
	return wp_mouse_button_changed(button) &&
		   !state.input.mouse.buttons_current[button];
}

bool wp_mouse_button_changed(uint32_t button) {
	return state.input.mouse.buttons_current[button] !=
		   state.input.mouse.buttons_last[button];
}

bool wp_mouse_button_down_on_div(uint32_t button) {
	return wp_mouse_button_down(button) &&
		   state.scrollbar_div.id == state.current_div.id;
}

bool wp_mouse_button_released_on_div(uint32_t button) {
	return wp_mouse_button_up(button) &&
		   state.scrollbar_div.id == state.current_div.id;
}

bool wp_mouse_button_changed_on_div(uint32_t button) {
	return wp_mouse_button_changed(button) &&
		   state.scrollbar_div.id == state.current_div.id;
}
double wp_get_mouse_x() { return state.input.mouse.xpos; }

double wp_get_mouse_y() { return state.input.mouse.ypos; }

double wp_get_mouse_x_delta() { return state.input.mouse.xpos_delta; }

double wp_get_mouse_y_delta() { return state.input.mouse.ypos_delta; }

double wp_get_mouse_scroll_x() { return state.input.mouse.xscroll_delta; }

double wp_get_mouse_scroll_y() { return state.input.mouse.yscroll_delta; }

wp_div *_wp_div_begin_loc(vec2s pos, vec2s size, bool scrollable, float *scroll,
						  float *scroll_velocity, const char *file,
						  int32_t line) {
	bool hovered_div = wp_area_hovered(pos, size);
	if (hovered_div) {
		state.scroll_velocity_ptr = scroll_velocity;
		state.scroll_ptr = scroll;
	}
	uint64_t id = DJB2_INIT;
	id = djb2_hash(id, file, strlen(file));
	id = djb2_hash(id, &line, sizeof(line));
	if (state.element_id_stack != -1) {
		id = djb2_hash(id, &state.element_id_stack,
					   sizeof(state.element_id_stack));
	}

	state.prev_pos_ptr = state.pos_ptr;
	state.prev_font_stack = state.font_stack;
	state.prev_line_height = state.current_line_height;
	state.prev_div = state.current_div;

	wp_element_props props = get_props_for(state.theme.div_props);

	state.div_props = props;

	wp_div div;
	div.aabb = (wp_aabb){.pos = pos, .size = size};
	div.scrollable = scrollable;
	div.id = id;

	if (div.scrollable) {
		if (*scroll > 0)
			*scroll = 0;

		if (state.theme.div_smooth_scroll) {
			*scroll += *scroll_velocity;
			*scroll_velocity *= state.theme.div_scroll_velocity_deceleration;
			if (*scroll_velocity > -0.1 && state.div_velocity_accelerating) {
				*scroll_velocity = 0.0f;
			}
		}
	}

	state.pos_ptr = pos;
	state.current_div = div;

	div.interact_state = div_container(
		(vec2s){pos.x - props.padding, pos.y - props.padding},
		(vec2s){size.x + props.padding * 2.0f, size.y + props.padding * 2.0f},
		props, props.color, props.border_width, false, state.div_hoverable);

	if (hovered_div) {
		state.selected_div_tmp = div;
	}

	// Culling & Scrolling
	if (div.scrollable) {
		wp_set_ptr_y(*scroll + props.border_width + props.corner_radius);
	} else {
		wp_set_ptr_y(props.border_width + props.corner_radius);
	}
	state.cull_start = (vec2s){pos.x, pos.y + props.border_width};
	state.cull_end = (vec2s){pos.x + size.x - props.border_width,
							 pos.y + size.y - props.border_width};

	state.current_div = div;

	state.current_line_height = 0;
	state.font_stack = NULL;

	return &state.current_div;
}

void wp_div_end() {
	if (state.current_div.scrollable) {
		draw_scrollbar_on(&state.selected_div_tmp);
	}

	state.pos_ptr = state.prev_pos_ptr;
	state.font_stack = state.prev_font_stack;
	state.current_line_height = state.prev_line_height;
	state.current_div = state.prev_div;
	state.cull_start = (vec2s){-1, -1};
	state.cull_end = (vec2s){-1, -1};
}

wp_clickable_state _wp_item_loc(vec2s size, const char *file, int32_t line) {
	wp_element_props props = get_props_for(state.theme.button_props);

	next_line_on_overflow((vec2s){size.x + props.padding * 2.0f +
									  props.margin_right + props.margin_left,
								  size.y + props.padding * 2.0f +
									  props.margin_bottom + props.margin_top},
						  state.div_props.border_width);

	state.pos_ptr.x += props.margin_left;
	state.pos_ptr.y += props.margin_top;

	wp_clickable_state item =
		button(file, line, state.pos_ptr, size, props, props.color,
			   props.border_width, false, true);

	state.pos_ptr.x += size.x + props.margin_left + props.padding * 2.0f;
	state.pos_ptr.y -= props.margin_top;
	return item;
}
wp_clickable_state _wp_button_loc(const char *text, const char *file,
								  int32_t line) {
	return button_element_loc((void *)text, file, line, false);
}

wp_clickable_state _wp_button_wide_loc(const wchar_t *text, const char *file,
									   int32_t line) {
	return button_element_loc((void *)text, file, line, true);
}

wp_clickable_state _wp_image_button_loc(wp_texture img, const char *file,
										int32_t line) {
	// Retrieving the property data of the button
	wp_element_props props = get_props_for(state.theme.button_props);
	float padding = props.padding;
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;

	wp_color color = props.color;
	wp_color text_color = state.theme.button_props.text_color;
	wp_font font = get_current_font();

	// If the button does not fit onto the current div, advance to the next line
	next_line_on_overflow(
		(vec2s){img.width + padding * 2.0f, img.height + padding * 2.0f},
		state.div_props.border_width);

	// Advancing the position pointer by the margins
	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Rendering the button
	wp_clickable_state ret =
		button(file, line, state.pos_ptr,
			   (vec2s){img.width + padding * 2, img.height + padding * 2},
			   props, color, props.border_width, true, true);

	wp_color imageColor = WP_WHITE;
	wp_image_render(
		(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding},
		imageColor, img, WP_NO_COLOR, 0, props.corner_radius);

	// Advancing the position pointer by the width of the button
	state.pos_ptr.x += img.width + margin_right + padding * 2.0f;
	state.pos_ptr.y -= margin_top;

	return ret;
}

wp_clickable_state _wp_image_button_fixed_loc(wp_texture img, float width,
											  float height, const char *file,
											  int32_t line) {
	// Retrieving the property data of the button
	wp_element_props props = get_props_for(state.theme.button_props);
	float padding = props.padding;
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;

	wp_color color = props.color;
	wp_color text_color = state.theme.button_props.text_color;
	wp_font font = get_current_font();

	int32_t render_width = ((width == -1) ? img.width : width);
	int32_t render_height = ((height == -1) ? img.height : height);

	// If the button does not fit onto the current div, advance to the next line
	next_line_on_overflow(
		(vec2s){render_width + padding * 2.0f, render_height + padding * 2.0f},
		state.div_props.border_width);

	// Advancing the position pointer by the margins
	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Rendering the button
	wp_clickable_state ret =
		button(file, line, state.pos_ptr,
			   (vec2s){render_width + padding * 2, render_height + padding * 2},
			   props, color, props.border_width, true, true);
	wp_color imageColor = WP_WHITE;
	wp_image_render(
		(vec2s){state.pos_ptr.x + padding + (render_width - img.width) / 2.0f,
				state.pos_ptr.y + padding},
		imageColor, img, WP_NO_COLOR, 0, props.corner_radius);

	// Advancing the position pointer by the width of the button
	state.pos_ptr.x += render_width + margin_right + padding * 2.0f;
	state.pos_ptr.y -= margin_top;

	return ret;
}

wp_clickable_state _wp_button_fixed_loc(const char *text, float width,
										float height, const char *file,
										int32_t line) {
	return button_fixed_element_loc((void *)text, width, height, file, line,
									false);
}

wp_clickable_state _wp_button_fixed_loc_wide(const wchar_t *text, float width,
											 float height, const char *file,
											 int32_t line) {
	return button_fixed_element_loc((void *)text, width, height, file, line,
									true);
}

wp_clickable_state _wp_slider_int_loc(wp_slider *slider, const char *file,
									  int32_t line) {
	// Getting property data
	wp_element_props props = get_props_for(state.theme.button_props);
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;

	float handle_size;
	if (slider->handle_size != 0.0f)
		handle_size = slider->handle_size;
	else
		handle_size = (slider->height != 0) ? slider->height * 4 : 20;

	if (slider->held) {
		handle_size = (slider->height != 0) ? slider->height * 4.5 : 22.5;
	}
	float slider_width = (slider->width != 0) ? slider->width : 200;
	float slider_height =
		(slider->height != 0) ? slider->height : handle_size / 2.0f;

	wp_color color = props.color;

	next_line_on_overflow((vec2s){slider_width + margin_right + margin_left,
								  handle_size + margin_bottom + margin_top},
						  state.div_props.border_width);

	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Render the slider
	wp_element_props slider_props = props;
	slider_props.border_width /= 2.0f;
	wp_clickable_state slider_state = button_ex(
		file, line, state.pos_ptr,
		(vec2s){(float)slider_width, (float)slider_height}, slider_props, color,
		0, false, false, (vec2s){-1, handle_size});

	slider->handle_pos =
		map_vals(*(int32_t *)slider->val, slider->min, slider->max,
				 handle_size / 2.0f, slider->width - handle_size / 2.0f) -
		(handle_size) / 2.0f;

	wp_rect_render(
		(vec2s){state.pos_ptr.x + slider->handle_pos,
				state.pos_ptr.y - (handle_size) / 2.0f + slider_height / 2.0f},
		(vec2s){handle_size, handle_size}, props.text_color, props.border_color,
		props.border_width,
		slider->held ? props.corner_radius * 3.5f : props.corner_radius * 3.0f);

	// Check if the slider bar is pressed

	if (slider_state == WP_HELD || slider_state == WP_CLICKED) {
		slider->held = true;
	}
	if (slider->held && wp_mouse_button_up(GLFW_MOUSE_BUTTON_LEFT)) {
		slider->held = false;
		slider_state = WP_CLICKED;
	}
	if (slider->held) {
		if (wp_get_mouse_x() >= state.pos_ptr.x &&
			wp_get_mouse_x() <= state.pos_ptr.x + slider_width - handle_size) {
			slider->handle_pos = wp_get_mouse_x() - state.pos_ptr.x;
			*(int32_t *)slider->val =
				map_vals(state.pos_ptr.x + slider->handle_pos, state.pos_ptr.x,
						 state.pos_ptr.x + slider_width - handle_size,
						 slider->min, slider->max);
		} else if (wp_get_mouse_x() <= state.pos_ptr.x) {
			*(int32_t *)slider->val = slider->min;
			slider->handle_pos = 0;
		} else if (wp_get_mouse_x() >=
				   state.pos_ptr.x + slider_width - handle_size) {
			*(int32_t *)slider->val = slider->max;
			slider->handle_pos = slider_width - handle_size;
		}
		slider_state = WP_HELD;
	}
	state.pos_ptr.x += slider_width + margin_right;
	state.pos_ptr.y -= margin_top;

	return slider_state;
}

wp_clickable_state _wp_progress_bar_val_loc(float width, float height,
											int32_t min, int32_t max,
											int32_t val, const char *file,
											int32_t line) {
	// Getting property data
	wp_element_props props = get_props_for(state.theme.slider_props);
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;
	// constants
	const float handle_size = (height == -1) ? 10 : height * 2; // px
	const float slider_width = (width == -1) ? 200 : width;		// px
	const float slider_height =
		(height == -1) ? handle_size / 2.0f : height; // px
	// Get the height of the element

	next_line_on_overflow((vec2s){slider_width + margin_right + margin_left,
								  handle_size + margin_bottom + margin_top},
						  state.div_props.border_width);

	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top + handle_size / 4.0f;

	// Render the slider
	wp_element_props slider_props = props;
	slider_props.corner_radius = props.corner_radius / 2.0f;
	wp_clickable_state slider_state =
		button(file, line, state.pos_ptr,
			   (vec2s){(float)slider_width, (float)slider_height}, slider_props,
			   props.color, 0, false, false);

	// Render the handle
	int32_t handle_pos = map_vals(val, min, max, handle_size / 2.0f,
								  slider_width - handle_size / 2.0f) -
						 (handle_size) / 2.0f;

	wp_push_element_id(1);
	wp_clickable_state handle = button(
		file, line,
		(vec2s){state.pos_ptr.x + handle_pos,
				state.pos_ptr.y - (handle_size) / 2.0f + slider_height / 2.0f},
		(vec2s){handle_size, handle_size}, props, props.text_color,
		props.border_width, false, false);
	wp_pop_element_id();

	state.pos_ptr.x += slider_width + margin_right;
	state.pos_ptr.y -= margin_top + handle_size / 4.0f;
	return handle;
}
wp_clickable_state _wp_progress_bar_int_loc(float val, float min, float max,
											float width, float height,
											const char *file, int32_t line) {
	// Getting property data
	wp_element_props props = get_props_for(state.theme.slider_props);
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;
	wp_color color = props.color;
	// constants

	next_line_on_overflow((vec2s){width + margin_right + margin_left,
								  height + margin_bottom + margin_top},
						  state.div_props.border_width);

	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Render the slider
	wp_clickable_state bar =
		button(file, line, state.pos_ptr, (vec2s){(float)width, (float)height},
			   props, color, props.border_width, false, false);

	float pos_x = map_vals(val, min, max, 0, width);

	wp_push_element_id(1);
	wp_clickable_state handle =
		button(file, line, state.pos_ptr, (vec2s){(float)pos_x, (float)height},
			   props, props.text_color, 0, false, false);
	wp_pop_element_id();

	state.pos_ptr.x += width + margin_right;
	state.pos_ptr.y -= margin_top;
	return bar;
}

wp_clickable_state _wp_progress_stripe_int_loc(wp_slider *slider,
											   const char *file, int32_t line) {
	// Getting property data
	wp_element_props props = get_props_for(state.theme.slider_props);
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;

	const float handle_size = 20; // px
	const float height =
		(slider->height != 0) ? slider->height : handle_size / 2.0f; // px

	wp_color color = props.color;

	next_line_on_overflow((vec2s){slider->width + margin_right + margin_left,
								  slider->height + margin_bottom + margin_top},
						  state.div_props.border_width);

	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Render the slider
	wp_clickable_state bar = button(
		file, line, state.pos_ptr, (vec2s){(float)slider->width, (float)height},
		props, color, props.border_width, false, false);

	// Check if the slider bar is pressed
	slider->handle_pos = map_vals(*(int32_t *)slider->val, slider->min,
								  slider->max, 0, slider->width);

	wp_push_element_id(1);
	wp_clickable_state handle =
		button(file, line, state.pos_ptr,
			   (vec2s){(float)slider->handle_pos, (float)height}, props,
			   props.text_color, 0, false, false);
	wp_pop_element_id();

	wp_rect_render(
		(vec2s){state.pos_ptr.x + slider->handle_pos,
				state.pos_ptr.y - (float)height / 2.0f},
		(vec2s){(float)slider->height * 2, (float)slider->height * 2},
		props.text_color, (wp_color){0.0f, 0.0f, 0.0f, 0.0f}, 0,
		props.corner_radius);

	state.pos_ptr.x += slider->width + margin_right;
	state.pos_ptr.y -= margin_top;
	return bar;
}

wp_clickable_state _wp_checkbox_loc(const char *text, bool *val,
									wp_color tick_color, wp_color tex_color,
									const char *file, int32_t line) {
	return checkbox_element_loc((void *)text, val, tick_color, tex_color, file,
								line, false);
}

wp_clickable_state _wp_checkbox_wide_loc(const wchar_t *text, bool *val,
										 wp_color tick_color,
										 wp_color tex_color, const char *file,
										 int32_t line) {
	return checkbox_element_loc((void *)text, val, tick_color, tex_color, file,
								line, true);
}

int32_t _wp_menu_item_list_loc(const char **items, uint32_t item_count,
							   int32_t selected_index,
							   wp_menu_item_callback per_cb, bool vertical,
							   const char *file, int32_t line) {
	return menu_item_list_item_loc((void **)items, item_count, selected_index,
								   per_cb, vertical, file, line, false);
}

int32_t _wp_menu_item_list_loc_wide(const wchar_t **items, uint32_t item_count,
									int32_t selected_index,
									wp_menu_item_callback per_cb, bool vertical,
									const char *file, int32_t line) {
	return menu_item_list_item_loc((void **)items, item_count, selected_index,
								   per_cb, vertical, file, line, true);
}

void _wp_dropdown_menu_loc(const char **items, const char *placeholder,
						   uint32_t item_count, float width, float height,
						   int32_t *selected_index, bool *opened,
						   const char *file, int32_t line) {
	return dropdown_menu_item_loc((void **)items, (void *)placeholder,
								  item_count, width, height, selected_index,
								  opened, file, line, false);
}

void _wp_dropdown_menu_loc_wide(const wchar_t **items,
								const wchar_t *placeholder, uint32_t item_count,
								float width, float height,
								int32_t *selected_index, bool *opened,
								const char *file, int32_t line) {
	return dropdown_menu_item_loc((void **)items, (void *)placeholder,
								  item_count, width, height, selected_index,
								  opened, file, line, true);
}

void _wp_input_text_loc(wp_input_field *input, const char *file, int32_t line) {
	input_field(input, INPUT_TEXT, file, line);
}

void _wp_input_int_loc(wp_input_field *input, const char *file, int32_t line) {
	input_field(input, INPUT_INT, file, line);
}
void _wp_input_float_loc(wp_input_field *input, const char *file,
						 int32_t line) {
	input_field(input, INPUT_FLOAT, file, line);
}
void wp_input_insert_char_idx(wp_input_field *input, char c, uint32_t idx) {
	wp_input_field_unselect_all(input);
	insert_i_str(input->buf, c, idx);
}

void wp_input_insert_str_idx(wp_input_field *input, const char *insert,
							 uint32_t len, uint32_t idx) {
	if (len > input->buf_size || strlen(input->buf) + len > input->buf_size)
		return;

	insert_str_str(input->buf, insert, idx);
	wp_input_field_unselect_all(input);
}

void wp_input_field_unselect_all(wp_input_field *input) {
	input->selection_start = -1;
	input->selection_end = -1;
	input->selection_dir = 0;
}

bool wp_input_grabbed() { return state.input_grabbed; }

void wp_div_grab(wp_div div) { state.grabbed_div = div; }

void wp_div_ungrab() {
	memset(&state.grabbed_div, 0, sizeof(wp_div));
	state.grabbed_div.id = -1;
}

bool wp_div_grabbed() { return state.grabbed_div.id != -1; }

wp_div wp_get_grabbed_div() { return state.grabbed_div; }

void _wp_begin_loc(const char *file, int32_t line) {
	state.pos_ptr = (vec2s){0, 0};
	renderer_begin();
	wp_element_props props = get_props_for(state.theme.div_props);
	props.color = (wp_color){0, 0, 0, 0};
	wp_push_style_props(props);

	wp_div_begin(((vec2s){0, 0}),
				 ((vec2s){(float)state.dsp_w, (float)state.dsp_h}), true);

	wp_pop_style_props();
}
void wp_end() {
	wp_div_end();

	state.selected_div = state.selected_div_tmp;

	update_input();
	clear_events();
	renderer_flush();
	state.drawcalls = 0;
}

void wp_next_line() {
	state.pos_ptr.x =
		state.current_div.aabb.pos.x + state.div_props.border_width;
	state.pos_ptr.y += state.current_line_height;
	state.current_line_height = 0;
}
vec2s wp_text_dimension(const char *str) {
	return wp_text_dimension_ex(str, -1);
}

vec2s wp_text_dimension_ex(const char *str, float wrap_point) {
	wp_font font = get_current_font();
	wp_text_props props = wp_text_render(
		(vec2s){0.0f, 0.0f}, str, font, state.theme.text_props.text_color,
		wrap_point, (vec2s){-1, -1}, true, false, -1, -1);

	return (vec2s){(float)props.width, (float)props.height};
}

vec2s wp_text_dimension_wide(const wchar_t *str) {
	return wp_text_dimension_wide_ex(str, -1);
}

vec2s wp_text_dimension_wide_ex(const wchar_t *str, float wrap_point) {
	wp_font font = get_current_font();
	wp_text_props props =
		wp_text_render_wchar((vec2s){0.0f, 0.0f}, str, font, WP_NO_COLOR,
							 wrap_point, (vec2s){-1, -1}, true, false, -1, -1);

	return (vec2s){(float)props.width, (float)props.height};
}

vec2s wp_button_dimension(const char *text) {
	wp_element_props props = get_props_for(state.theme.button_props);
	float padding = props.padding;
	vec2s text_dimension = wp_text_dimension(text);
	return (vec2s){text_dimension.x + padding * 2.0f,
				   text_dimension.y + padding};
}

float wp_get_text_end(const char *str, float start_x) {
	wp_font font = get_current_font();
	wp_text_props props =
		text_render_simple((vec2s){start_x, 0.0f}, str, font,
						   state.theme.text_props.text_color, true);
	return props.end_x;
}

void wp_text(const char *text) {
	// Retrieving the property data of the text
	wp_element_props props = get_props_for(state.theme.text_props);
	float padding = props.padding;
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;

	wp_color text_color = props.text_color;
	wp_color color = props.color;
	wp_font font = get_current_font();

	// Advancing to the next line if the the text does not fit on the current
	// div
	wp_text_props text_props = wp_text_render(
		state.pos_ptr, text, font, text_color,
		state.text_wrap
			? (state.current_div.aabb.size.x + state.current_div.aabb.pos.x) -
				  margin_right - margin_left
			: -1,
		(vec2s){-1, -1}, true, false, -1, -1);
	next_line_on_overflow(
		(vec2s){text_props.width + padding * 2.0f + margin_left + margin_right,
				text_props.height + padding * 2.0f + margin_top +
					margin_bottom},
		state.div_props.border_width);

	// Advancing the position pointer by the margins
	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Rendering a colored text box if a color is specified
	// Rendering the text
	wp_text_render(
		(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding}, text,
		font, text_color,
		state.text_wrap
			? (state.current_div.aabb.size.x + state.current_div.aabb.pos.x) -
				  margin_right - margin_left
			: -1,
		(vec2s){-1, -1}, false, false, -1, -1);

	// Advancing the position pointer by the width of the text
	state.pos_ptr.x += text_props.width + margin_right + padding;
	state.pos_ptr.y -= margin_top;
}

void wp_text_wide(const wchar_t *text) {
	// Retrieving the property data of the text
	wp_element_props props = get_props_for(state.theme.text_props);
	float padding = props.padding;
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;
	wp_color text_color = props.text_color;
	wp_color color = props.color;
	wp_font font = get_current_font();

	// Advancing to the next line if the the text does not fit on the current
	// div
	wp_text_props text_props = wp_text_render_wchar(
		state.pos_ptr, text, font, text_color,
		(state.text_wrap
			 ? (state.current_div.aabb.size.x + state.current_div.aabb.pos.x) -
				   margin_right - margin_left
			 : -1),
		(vec2s){-1, -1}, true, false, -1, -1);
	next_line_on_overflow(
		(vec2s){text_props.width + padding * 2.0f + margin_left + margin_right,
				text_props.height + padding * 2.0f + margin_top +
					margin_bottom},
		state.div_props.border_width);

	// Advancing the position pointer by the margins

	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	wp_rect_render(state.pos_ptr,
				   (vec2s){text_props.width + padding * 2.0f,
						   text_props.height + padding * 2.0f},
				   props.color, props.border_color, props.border_width,
				   props.corner_radius);

	// Rendering a colored text box if a color is specified
	// Rendering the text
	wp_text_render_wchar(
		(vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding}, text,
		font, text_color,
		(state.text_wrap
			 ? (state.current_div.aabb.size.x + state.current_div.aabb.pos.x) -
				   margin_right - margin_left
			 : -1),
		(vec2s){-1, -1}, false, false, -1, -1);

	// Advancing the position pointer by the width of the text
	state.pos_ptr.x +=
		text_props.width + padding * 2.0f + margin_right + padding;
	state.pos_ptr.y -= margin_top;
}

void wp_set_text_wrap(bool wrap) { state.text_wrap = wrap; }

wp_div wp_get_current_div() { return state.current_div; }

wp_div wp_get_selected_div() { return state.selected_div; }

wp_div *wp_get_current_div_ptr() { return &state.current_div; }

wp_div *wp_get_selected_div_ptr() { return &state.selected_div; }

void wp_set_ptr_x(float x) {
	state.pos_ptr.x = x + state.current_div.aabb.pos.x;
}

void wp_set_ptr_y(float y) {
	state.pos_ptr.y = y + state.current_div.aabb.pos.y;
}
void wp_set_ptr_x_absolute(float x) { state.pos_ptr.x = x; }

void wp_set_ptr_y_absolute(float y) { state.pos_ptr.y = y; }
float wp_get_ptr_x() { return state.pos_ptr.x; }

float wp_get_ptr_y() { return state.pos_ptr.y; }

uint32_t wp_get_display_width() { return state.dsp_w; }

uint32_t wp_get_display_height() { return state.dsp_h; }

void wp_push_font(wp_font *font) { state.font_stack = font; }

void wp_pop_font() { state.font_stack = NULL; }

// Decode a UTF-8 character sequence to Unicode code point
uint32_t decode_utf8(const char *s, int *bytes_read) {
	uint8_t c = s[0];
	if (c < 0x80) {
		*bytes_read = 1;
		return c;
	} else if (c < 0xE0) {
		*bytes_read = 2;
		return ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
	} else if (c < 0xF0) {
		*bytes_read = 3;
		return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
	} else {
		*bytes_read = 4;
		return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
			   ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
	}
}

static void renderer_add_glyph(stbtt_aligned_quad q,
							   int32_t max_descended_char_height,
							   wp_color color, uint32_t tex_index) {
	vec2s texcoords[4] = {q.s0, q.t0, q.s1, q.t0, q.s1, q.t1, q.s0, q.t1};
	vec2s verts[4] = {(vec2s){q.x0, q.y0 + max_descended_char_height},
					  (vec2s){q.x1, q.y0 + max_descended_char_height},
					  (vec2s){q.x1, q.y1 + max_descended_char_height},
					  (vec2s){q.x0, q.y1 + max_descended_char_height}};
	for (uint32_t i = 0; i < 4; i++) {
		if (state.render.vert_count >= MAX_RENDER_BATCH) {
			renderer_flush();
			renderer_begin();
		}
		const vec2 verts_arr = {verts[i].x, verts[i].y};
		memcpy(state.render.verts[state.render.vert_count].pos, verts_arr,
			   sizeof(vec2));

		const vec4 border_color = {0, 0, 0, 0};
		memcpy(state.render.verts[state.render.vert_count].border_color,
			   border_color, sizeof(vec4));

		state.render.verts[state.render.vert_count].border_width = 0;

		vec4s color_zto = wp_color_to_zto(color);
		const vec4 color_arr = {color_zto.r, color_zto.g, color_zto.b,
								color_zto.a};
		memcpy(state.render.verts[state.render.vert_count].color, color_arr,
			   sizeof(vec4));

		const vec2 texcoord_arr = {texcoords[i].x, texcoords[i].y};
		memcpy(state.render.verts[state.render.vert_count].texcoord,
			   texcoord_arr, sizeof(vec2));

		state.render.verts[state.render.vert_count].tex_index = tex_index;

		const vec2 scale_arr = {0, 0};
		memcpy(state.render.verts[state.render.vert_count].scale, scale_arr,
			   sizeof(vec2));

		const vec2 pos_px_arr = {0, 0};
		memcpy(state.render.verts[state.render.vert_count].pos_px, pos_px_arr,
			   sizeof(vec2));

		state.render.verts[state.render.vert_count].corner_radius = 0;

		const vec2 cull_start_arr = {state.cull_start.x, state.cull_start.y};
		const vec2 cull_end_arr = {state.cull_end.x, state.cull_end.y};
		memcpy(state.render.verts[state.render.vert_count].min_coord,
			   cull_start_arr, sizeof(vec2));
		memcpy(state.render.verts[state.render.vert_count].max_coord,
			   cull_end_arr, sizeof(vec2));

		state.render.vert_count++;
	}
	state.render.index_count += 6;
}

static wchar_t *str_to_wstr(const char *str) {
	size_t len = strlen(str) + 1;

	wchar_t *wstr = (wchar_t *)malloc(len * sizeof(wchar_t));
	if (wstr == NULL) {
		perror("Memory allocation failed");
		return NULL;
	}
	if (mbstowcs(wstr, str, len) == (size_t)-1) {
		perror("Conversion failed");
		free(wstr);
		return NULL;
	}

	return wstr;
}

wp_text_props wp_text_render(vec2s pos, const char *str, wp_font font,
							 wp_color color, int32_t wrap_point,
							 vec2s stop_point, bool no_render,
							 bool render_solid, int32_t start_index,
							 int32_t end_index) {
	wchar_t *wstr = str_to_wstr(str);
	wp_text_props textprops = wp_text_render_wchar(
		pos, (const wchar_t *)wstr, font, color, wrap_point, stop_point,
		no_render, render_solid, start_index, end_index);
	free(wstr);
	return textprops;
}

wp_text_props wp_text_render_wchar(vec2s pos, const wchar_t *str, wp_font font,
								   wp_color color, int32_t wrap_point,
								   vec2s stop_point, bool no_render,
								   bool render_solid, int32_t start_index,
								   int32_t end_index) {
	bool culled = item_should_cull(
		(wp_aabb){.pos = (vec2s){pos.x, pos.y + get_current_font().font_size},
				  .size = (vec2s){-1, -1}});

	// Retrieving the texture index
	float tex_index = -1.0f;
	if (!culled && !no_render) {
		if (state.render.tex_count - 1 >= MAX_TEX_COUNT_BATCH - 1) {
			renderer_flush();
			renderer_begin();
		}
		for (uint32_t i = 0; i < state.render.tex_count; i++) {
			if (state.render.textures[i].id == font.texture.id) {
				tex_index = (float)i;
				break;
			}
		}
		if (tex_index == -1.0f) {
			tex_index = (float)state.render.tex_index;
			wp_texture tex = font.texture;
			state.render.textures[state.render.tex_count++] = tex;
			state.render.tex_index++;
		}
	}

	// Local variables needed for rendering
	wp_text_props ret = {0};
	float x = pos.x;
	float y = pos.y;

	int32_t max_descended_char_height = get_max_char_height_font(font);

	float last_x = x;

	float height = get_max_char_height_font(font);
	float width = 0;

	uint32_t i = 0;
	while (str[i] != L'\0') {
		if (str[i] >= font.num_glyphs) {
			i++;
			continue;
		}
		if (stbtt_FindGlyphIndex((const stbtt_fontinfo *)font.font_info,
								 str[i] - 32) == 0 &&
			str[i] != L' ' && str[i] != L'\n' && str[i] != L'\t' &&
			!iswdigit(str[i]) && !iswpunct(str[i])) {
			i++;
			continue;
		}
		if (i >= end_index && end_index != -1) {
			break;
		}

		// Calculate the width of the next word
		float word_width = 0;
		uint32_t j = i;
		while (str[j] != L' ' && str[j] != L'\n' && str[j] != L'\0') {
			stbtt_aligned_quad q;
			stbtt_GetBakedQuad((stbtt_bakedchar *)font.cdata, font.tex_width,
							   font.tex_height, str[j] - 32, &word_width, &y,
							   &q, 0);
			j++;
		}

		// If the next word exceeds the wrap point, move to the next line
		if (x + word_width > wrap_point && wrap_point != -1) {
			y += font.font_size;
			height += font.font_size;
			if (x - pos.x > width) {
				width = x - pos.x;
			}
			x = pos.x;
			last_x = x;
		}

		// If the current character is a new line, advance to the next line
		if (str[i] == L'\n') {
			y += font.font_size;
			height += font.font_size;
			if (x - pos.x > width) {
				width = x - pos.x;
			}
			x = pos.x;
			last_x = x;
			i++;
			continue;
		}

		// Retrieving the vertex data of the current character & submitting it
		// to the batch
		stbtt_aligned_quad q;
		stbtt_GetBakedQuad((stbtt_bakedchar *)font.cdata, font.tex_width,
						   font.tex_height, str[i] - 32, &x, &y, &q, 1);
		if (i < start_index && start_index != -1) {
			last_x = x;
			ret.rendered_count++;
			i++;
			continue;
		}
		if (stop_point.x != -1 && stop_point.y != -1) {
			if (x >= stop_point.x && stop_point.x != -1 &&
				y + get_max_char_height_font(font) >= stop_point.y &&
				stop_point.y != -1) {
				break;
			}
		} else {
			if (y + get_max_char_height_font(font) >= stop_point.y &&
				stop_point.y != -1) {
				break;
			}
		}
		if (!culled && !no_render && state.renderer_render) {
			if (render_solid) {
				wp_rect_render(
					(vec2s){x, y},
					(vec2s){last_x - x, get_max_char_height_font(font)}, color,
					WP_NO_COLOR, 0.0f, 0.0f);
			} else {
				renderer_add_glyph(q, max_descended_char_height, color,
								   tex_index);
			}
			last_x = x;
		}
		ret.rendered_count++;
		i++;
	}

	// Populating the return value
	if (x - pos.x > width) {
		width = x - pos.x;
	}
	ret.width = width;
	ret.height = height;
	ret.end_x = x;
	ret.end_y = y;
	return ret;
}

void wp_rect_render(vec2s pos, vec2s size, wp_color color,
					wp_color border_color, float border_width,
					float corner_radius) {
	if (!state.renderer_render)
		return;
	if (item_should_cull((wp_aabb){.pos = pos, .size = size})) {
		return;
	}
	// Offsetting the postion, so that pos is the top left of the rendered
	// object
	vec2s pos_initial = pos;
	pos = (vec2s){pos.x + size.x / 2.0f, pos.y + size.y / 2.0f};

	// Initializing texture coords data
	vec2s texcoords[4] = {
		(vec2s){1.0f, 1.0f},
		(vec2s){1.0f, 0.0f},
		(vec2s){0.0f, 0.0f},
		(vec2s){0.0f, 1.0f},
	};
	// Calculating the transform matrix
	mat4 translate;
	mat4 scale;
	mat4 transform;
	vec3 pos_xyz = {(corner_radius != 0.0f ? (float)state.dsp_w / 2.0f : pos.x),
					(corner_radius != 0.0f ? (float)state.dsp_h / 2.0f : pos.y),
					0.0f};
	vec3 size_xyz = {corner_radius != 0.0f ? state.dsp_w : size.x,
					 corner_radius != 0.0f ? state.dsp_h : size.y, 0.0f};
	glm_translate_make(translate, pos_xyz);
	glm_scale_make(scale, size_xyz);
	glm_mat4_mul(translate, scale, transform);

	// Adding the vertices to the batch renderer
	for (uint32_t i = 0; i < 4; i++) {
		if (state.render.vert_count >= MAX_RENDER_BATCH) {
			renderer_flush();
			renderer_begin();
		}
		vec4 result;
		glm_mat4_mulv(transform, state.render.vert_pos[i].raw, result);
		state.render.verts[state.render.vert_count].pos[0] = result[0];
		state.render.verts[state.render.vert_count].pos[1] = result[1];

		vec4s border_color_zto = wp_color_to_zto(border_color);
		const vec4 border_color_arr = {border_color_zto.r, border_color_zto.g,
									   border_color_zto.b, border_color_zto.a};
		memcpy(state.render.verts[state.render.vert_count].border_color,
			   border_color_arr, sizeof(vec4));

		state.render.verts[state.render.vert_count].border_width = border_width;

		vec4s color_zto = wp_color_to_zto(color);
		const vec4 color_arr = {color_zto.r, color_zto.g, color_zto.b,
								color_zto.a};
		memcpy(state.render.verts[state.render.vert_count].color, color_arr,
			   sizeof(vec4));

		const vec2 texcoord_arr = {texcoords[i].x, texcoords[i].y};
		memcpy(state.render.verts[state.render.vert_count].texcoord,
			   texcoord_arr, sizeof(vec2));

		state.render.verts[state.render.vert_count].tex_index = -1;

		const vec2 scale_arr = {size.x, size.y};
		memcpy(state.render.verts[state.render.vert_count].scale, scale_arr,
			   sizeof(vec2));

		const vec2 pos_px_arr = {(float)pos_initial.x, (float)pos_initial.y};
		memcpy(state.render.verts[state.render.vert_count].pos_px, pos_px_arr,
			   sizeof(vec2));

		state.render.verts[state.render.vert_count].corner_radius =
			corner_radius;

		const vec2 cull_start_arr = {state.cull_start.x, state.cull_start.y};
		memcpy(state.render.verts[state.render.vert_count].min_coord,
			   cull_start_arr, sizeof(vec2));

		const vec2 cull_end_arr = {state.cull_end.x, state.cull_end.y};
		memcpy(state.render.verts[state.render.vert_count].max_coord,
			   cull_end_arr, sizeof(vec2));

		state.render.vert_count++;
	}
	state.render.index_count += 6;
}

void wp_image_render(vec2s pos, wp_color color, wp_texture tex,
					 wp_color border_color, float border_width,
					 float corner_radius) {
	if (!state.renderer_render)
		return;
	if (item_should_cull(
			(wp_aabb){.pos = pos, .size = (vec2s){tex.width, tex.height}})) {
		return;
	}
	if (state.render.tex_count - 1 >= MAX_TEX_COUNT_BATCH - 1) {
		renderer_flush();
		renderer_begin();
	}
	// Offsetting the postion, so that pos is the top left of the rendered
	// object
	vec2s pos_initial = pos;
	pos = (vec2s){pos.x + tex.width / 2.0f, pos.y + tex.height / 2.0f};

	if (state.image_color_stack.a != 0.0) {
		color = state.image_color_stack;
	}
	// Initializing texture coords data
	vec2s texcoords[4] = {
		(vec2s){0.0f, 0.0f},
		(vec2s){1.0f, 0.0f},
		(vec2s){1.0f, 1.0f},
		(vec2s){0.0f, 1.0f},
	};
	// Retrieving the texture index of the rendered texture
	float tex_index = -1.0f;
	for (uint32_t i = 0; i < state.render.tex_count; i++) {
		if (tex.id == state.render.textures[i].id) {
			tex_index = i;
			break;
		}
	}
	if (tex_index == -1.0f) {
		tex_index = (float)state.render.tex_index;
		state.render.textures[state.render.tex_count++] = tex;
		state.render.tex_index++;
	}
	// Calculating the transform
	mat4 translate = GLM_MAT4_IDENTITY_INIT;
	mat4 scale = GLM_MAT4_IDENTITY_INIT;
	mat4 transform = GLM_MAT4_IDENTITY_INIT;
	vec3s pos_xyz = (vec3s){pos.x, pos.y, 0.0f};
	vec3 tex_size;
	tex_size[0] = tex.width;
	tex_size[1] = tex.height;
	tex_size[2] = 0;
	glm_translate_make(translate, pos_xyz.raw);
	glm_scale_make(scale, tex_size);
	glm_mat4_mul(translate, scale, transform);

	// Adding the vertices to the batch renderer
	for (uint32_t i = 0; i < 4; i++) {
		if (state.render.vert_count >= MAX_RENDER_BATCH) {
			renderer_flush();
			renderer_begin();
		}
		vec4 result;
		glm_mat4_mulv(transform, state.render.vert_pos[i].raw, result);
		memcpy(state.render.verts[state.render.vert_count].pos, result,
			   sizeof(vec2));

		vec4s border_color_zto = wp_color_to_zto(border_color);
		const vec4 border_color_arr = {border_color_zto.r, border_color_zto.g,
									   border_color_zto.b, border_color_zto.a};
		memcpy(state.render.verts[state.render.vert_count].border_color,
			   border_color_arr, sizeof(vec4));

		state.render.verts[state.render.vert_count].border_width = border_width;

		vec4s color_zto = wp_color_to_zto(color);
		const vec4 color_arr = {color_zto.r, color_zto.g, color_zto.b,
								color_zto.a};
		memcpy(state.render.verts[state.render.vert_count].color, color_arr,
			   sizeof(vec4));

		const vec2 texcoord_arr = {texcoords[i].x, texcoords[i].y};
		memcpy(state.render.verts[state.render.vert_count].texcoord,
			   texcoord_arr, sizeof(vec2));

		state.render.verts[state.render.vert_count].tex_index = tex_index;

		const vec2 scale_arr = {(float)tex.width, (float)tex.height};
		memcpy(state.render.verts[state.render.vert_count].scale, scale_arr,
			   sizeof(vec2));

		vec2 pos_px_arr = {(float)pos_initial.x, (float)pos_initial.y};
		memcpy(state.render.verts[state.render.vert_count].pos_px, pos_px_arr,
			   sizeof(vec2));

		state.render.verts[state.render.vert_count].corner_radius =
			corner_radius;

		const vec2 cull_start_arr = {state.cull_start.x, state.cull_start.y};
		memcpy(state.render.verts[state.render.vert_count].min_coord,
			   cull_start_arr, sizeof(vec2));

		const vec2 cull_end_arr = {state.cull_end.x, state.cull_end.y};
		memcpy(state.render.verts[state.render.vert_count].max_coord,
			   cull_end_arr, sizeof(vec2));

		state.render.vert_count++;
	}
	state.render.index_count += 6;
}

bool wp_point_intersects_aabb(vec2s p, wp_aabb aabb) {
	vec2s min = {aabb.pos.x, aabb.pos.y};
	vec2s max = {aabb.pos.x + aabb.size.x, aabb.pos.y + aabb.size.y};

	// Check if the p is within the AABB bounds
	if (p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y) {
		return true;
	}
	return false;
}

bool wp_aabb_intersects_aabb(wp_aabb a, wp_aabb b) {
	vec2s minA = a.pos;
	vec2s maxA = (vec2s){a.pos.x + a.size.x, a.pos.y + a.size.y};

	vec2s minB = b.pos;
	vec2s maxB = (vec2s){b.pos.x + b.size.x, b.pos.y + b.size.y};

	return (minA.x >= minB.x && minA.x <= maxB.x && minA.y >= minB.y &&
			minA.y <= maxB.y);
}

void wp_push_style_props(wp_element_props props) {
	props_stack_push(&state.props_stack, props);
}

void wp_pop_style_props() { props_stack_pop(&state.props_stack); }

bool wp_hovered(vec2s pos, vec2s size) {
	bool hovered =
		wp_get_mouse_x() <= (pos.x + size.x) && wp_get_mouse_x() >= (pos.x) &&
		wp_get_mouse_y() <= (pos.y + size.y) && wp_get_mouse_y() >= (pos.y) &&
		((state.selected_div.id == state.current_div.id &&
		  state.grabbed_div.id == -1) ||
		 (state.grabbed_div.id == state.current_div.id &&
		  state.grabbed_div.id != -1));
	return hovered;
}

bool wp_area_hovered(vec2s pos, vec2s size) {
	bool hovered =
		wp_get_mouse_x() <= (pos.x + size.x) && wp_get_mouse_x() >= (pos.x) &&
		wp_get_mouse_y() <= (pos.y + size.y) && wp_get_mouse_y() >= (pos.y);
	return hovered;
}

wp_cursor_pos_event wp_get_mouse_move_event() { return state.cp_ev; }

wp_mouse_button_event wp_get_mouse_button_event() { return state.mb_ev; }

wp_scroll_event wp_get_mouse_scroll_event() { return state.scr_ev; }

wp_key_event wp_get_key_event() { return state.key_ev; }

wp_char_event wp_get_char_event() { return state.ch_ev; }

void wp_set_cull_end_x(float x) { state.cull_end.x = x; }

void wp_set_cull_end_y(float y) { state.cull_end.y = y; }
void wp_set_cull_start_x(float x) { state.cull_start.x = x; }

void wp_set_cull_start_y(float y) { state.cull_start.y = y; }

void wp_unset_cull_start_x() { state.cull_start.x = -1; }

void wp_unset_cull_start_y() { state.cull_start.y = -1; }

void wp_unset_cull_end_x() { state.cull_end.x = -1; }

void wp_unset_cull_end_y() { state.cull_end.y = -1; }

void wp_set_image_color(wp_color color) { state.image_color_stack = color; }

void wp_unset_image_color() { state.image_color_stack = WP_NO_COLOR; }

void wp_set_current_div_scroll(float scroll) { *state.scroll_ptr = scroll; }
float wp_get_current_div_scroll() { return *state.scroll_ptr; }

void wp_set_current_div_scroll_velocity(float scroll_velocity) {
	*state.scroll_velocity_ptr = scroll_velocity;
}

float wp_get_current_div_scroll_velocity() { return *state.scroll_ptr; }

void wp_set_line_height(uint32_t line_height) {
	state.current_line_height = line_height;
}

uint32_t wp_get_line_height() { return state.current_line_height; }

void wp_set_line_should_overflow(bool overflow) {
	state.line_overflow = overflow;
}

void wp_set_div_hoverable(bool clickable) { state.div_hoverable = clickable; }
void wp_push_element_id(int64_t id) { state.element_id_stack = id; }

void wp_pop_element_id() { state.element_id_stack = -1; }

wp_color wp_color_brightness(wp_color color, float brightness) {
	uint32_t adjustedR = (int)(color.r * brightness);
	uint32_t adjustedG = (int)(color.g * brightness);
	uint32_t adjustedB = (int)(color.b * brightness);
	color.r = (unsigned char)(adjustedR > 255 ? 255 : adjustedR);
	color.g = (unsigned char)(adjustedG > 255 ? 255 : adjustedG);
	color.b = (unsigned char)(adjustedB > 255 ? 255 : adjustedB);
	return color;
}

wp_color wp_color_alpha(wp_color color, uint8_t a) {
	return (wp_color){color.r, color.g, color.b, a};
}

vec4s wp_color_to_zto(wp_color color) {
	return (vec4s){color.r / 255.0f, color.g / 255.0f, color.b / 255.0f,
				   color.a / 255.0f};
}
wp_color wp_color_from_hex(uint32_t hex) {
	wp_color color;
	color.r = (hex >> 16) & 0xFF;
	color.g = (hex >> 8) & 0xFF;
	color.b = hex & 0xFF;
	color.a = 255;
	return color;
}

wp_color wp_color_from_zto(vec4s zto) {
	return (wp_color){(uint8_t)(zto.r * 255.0f), (uint8_t)(zto.g * 255.0f),
					  (uint8_t)(zto.b * 255.0f), (uint8_t)(zto.a * 255.0f)};
}

void wp_image(wp_texture tex) {
	// Retrieving the property data of the image
	wp_element_props props = get_props_for(state.theme.image_props);
	float margin_left = props.margin_left, margin_right = props.margin_right,
		  margin_top = props.margin_top, margin_bottom = props.margin_bottom;
	wp_color color = props.color;

	// Advancing to the next line if the image does not fit on the current div
	next_line_on_overflow((vec2s){tex.width + margin_left + margin_right,
								  tex.height + margin_top + margin_bottom},
						  state.div_props.border_width);

	// Advancing the position pointer by the margins
	state.pos_ptr.x += margin_left;
	state.pos_ptr.y += margin_top;

	// Rendering the image
	wp_image_render(state.pos_ptr, color, tex, props.border_color,
					props.border_width, props.corner_radius);

	// Advancing the position pointer by the width of the image
	state.pos_ptr.x += tex.width + margin_right;
	state.pos_ptr.y -= margin_top;
}

void wp_rect(float width, float height, wp_color color, float corner_radius) {
	// Rendering the rect
	next_line_on_overflow((vec2s){(float)width, (float)height},
						  state.div_props.border_width);

	wp_rect_render(state.pos_ptr, (vec2s){(float)width, (float)height}, color,
				   (wp_color){0.0f, 0.0f, 0.0f, 0.0f}, 0, corner_radius);

	state.pos_ptr.x += width;
}

void wp_seperator() {
	wp_next_line();
	wp_element_props props = get_props_for(state.theme.button_props);
	state.pos_ptr.x += props.margin_left;
	state.pos_ptr.y += props.margin_top;

	const uint32_t seperator_height = 1;
	wp_set_line_height(props.margin_top + seperator_height +
					   props.margin_bottom);

	wp_rect_render(
		state.pos_ptr,
		(vec2s){state.current_div.aabb.size.x - props.margin_left * 2.0f,
				seperator_height},
		props.color, WP_NO_COLOR, 0, props.corner_radius);

	state.pos_ptr.y -= props.margin_top;
	wp_next_line();
}

void wp_set_clipboard_text(const char *text) {
	clipboard_set_text(state.clipboard, text);
}
char *wp_get_clipboard_text() { return clipboard_text(state.clipboard); }

void wp_set_no_render(bool no_render) { state.renderer_render = !no_render; }
