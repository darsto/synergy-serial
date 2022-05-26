/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>

#include "synergy_proto.h"
#include "common.h"
#include "config.h"
#include "serial.h"
#include "arduino_keylayout.h"

enum {
	KEYMASK_NONE = 0,
	KEYMASK_SHIFT = 1,
	KEYMASK_CTRL = 2,
	KEYMASK_ALT = 4,
	KEYMASK_WINDOWS = 16,
	KEYMASK_CAPSLOCK = 4096,
};

#define STR2TAG(str) \
	((uint32_t)(((str)[0] << 24) | ((str)[1] << 16) | ((str)[2] << 8) | ((str)[3])))

static uint8_t __attribute__((used))
read_uint8(struct synergy_proto_conn *conn)
{
	if (conn->recv_len < 1) {
		conn->recv_error = -EINVAL;
		return 0;
	}

	conn->recv_buf += 1;
	conn->recv_len -= 1;
	return *(uint8_t *)(conn->recv_buf - 1);
}

static int8_t __attribute__((used))
read_int8(struct synergy_proto_conn *conn)
{
	return (int8_t)read_uint8(conn);
}

static void __attribute__((used))
write_uint8(struct synergy_proto_conn *conn, uint8_t val)
{
	void *buf;

	assert(conn->resp_len + 1 < sizeof(conn->resp_buf));
	buf = conn->resp_buf + conn->resp_len;
	*(uint8_t *)buf = val;
	conn->resp_len += 1;
}

static void __attribute__((used))
write_int8(struct synergy_proto_conn *conn, int8_t val)
{
	write_uint8(conn, (uint8_t)val);
}

static uint16_t __attribute__((used))
read_uint16(struct synergy_proto_conn *conn)
{
	if (conn->recv_len < 2) {
		conn->recv_error = -EINVAL;
		return 0;

	}

	conn->recv_buf += 2;
	conn->recv_len -= 2;
	return ntohs(*(uint16_t *)(conn->recv_buf - 2));
}

static int16_t __attribute__((used))
read_int16(struct synergy_proto_conn *conn)
{
	return (int16_t)read_uint16(conn);
}

static void __attribute__((used))
write_uint16(struct synergy_proto_conn *conn, uint16_t val)
{
	void *buf;

	assert(conn->resp_len + 2 < sizeof(conn->resp_buf));
	buf = conn->resp_buf + conn->resp_len;
	*(uint16_t *)buf = htons(val);
	conn->resp_len += 2;
}

static void __attribute__((used))
write_int16(struct synergy_proto_conn *conn, int16_t val)
{
	write_uint16(conn, (uint16_t)val);
}

static uint32_t __attribute__((used))
read_uint32(struct synergy_proto_conn *conn)
{
	if (conn->recv_len < 4) {
		conn->recv_error = -EINVAL;
		return 0;
	}

	conn->recv_buf += 4;
	conn->recv_len -= 4;
	return ntohl(*(uint32_t *)(conn->recv_buf - 4));
}

static int32_t __attribute__((used))
read_int32(struct synergy_proto_conn *conn)
{
	return (int32_t)read_uint32(conn);
}

static void __attribute__((used))
write_uint32(struct synergy_proto_conn *conn, uint32_t val)
{
	void *buf;

	assert(conn->resp_len + 4 < sizeof(conn->resp_buf));
	buf = conn->resp_buf + conn->resp_len;
	*(uint32_t *)buf = htonl(val);
	conn->resp_len += 4;
}

static void __attribute__((used))
write_int32(struct synergy_proto_conn *conn, int32_t val)
{
	write_uint32(conn, (uint32_t)val);
}

static void __attribute__((used))
write_raw_string(struct synergy_proto_conn *conn, const char *str)
{
	void *buf;
	uint32_t len = strlen(str);

	assert(conn->resp_len + len < sizeof(conn->resp_buf));
	buf = conn->resp_buf + conn->resp_len;
	memcpy(buf, str, len);
	conn->resp_len += len;
}

static void __attribute__((used))
write_string(struct synergy_proto_conn *conn, const char *str)
{
	void *buf;
	uint32_t len = strlen(str);

	assert(conn->resp_len + len < sizeof(conn->resp_buf));
	write_uint32(conn, len);

	buf = conn->resp_buf + conn->resp_len;
	memcpy(buf, str, len);
	conn->resp_len += len;
}

static void __attribute__((used))
read_nbytes(struct synergy_proto_conn *conn, unsigned nbytes)
{
	if (conn->recv_len < nbytes) {
		conn->recv_error = -EINVAL;
		return;
	}

	conn->recv_buf += nbytes;
	conn->recv_len -= nbytes;
}

static void __attribute__((used))
clear_resp(struct synergy_proto_conn *conn)
{
	conn->resp_len = 4;
}

static void __attribute__((used))
flush_resp(struct synergy_proto_conn *conn)
{
	int rc;

	*(uint32_t *)conn->resp_buf = htonl(conn->resp_len - 4);

	rc = send(conn->fd, conn->resp_buf, conn->resp_len, 0);
	if (rc < 0) {
		perror("send");
		return;
	}
	conn->resp_len = 4;
}

#define EXIT_ON_RECV_ERROR(conn) \
({ \
	if ((conn)->recv_error) { \
			LOG(LOG_ERROR, "recv error: %d", (conn)->recv_error); \
			clear_resp((conn)); \
		return -1; \
	} \
})


#define EXIT_ON_INVALID_RECV_PKT(conn) \
({ \
	EXIT_ON_RECV_ERROR((conn)); \
	if ((conn)->recv_len != 0) { \
			LOG(LOG_ERROR, "recv packet too long: %d bytes remaining", (conn)->recv_len); \
			clear_resp((conn)); \
		return -1; \
	} \
})

static void
init_synergy_proto_conn(struct synergy_proto_conn *conn)
{
	conn->resp_len = 4;
}

int
synergy_proto_handle_greeting(struct synergy_proto_conn *conn)
{
	const char *magicstr = "Synergy";

	init_synergy_proto_conn(conn);

	if (conn->recv_len != strlen(magicstr) + 4) {
		LOG(LOG_ERROR, "invalid pkt len (got %d bytes, expected %d)",
				conn->recv_len, strlen(magicstr) + 4);
		return -1;
	}

	if (strncmp(magicstr, conn->recv_buf, strlen(magicstr)) != 0) {
		fprintf(stderr, "recv greeting with wrong magic");
		return -1;
	}
	conn->recv_buf += strlen(magicstr);
	conn->recv_len -= strlen(magicstr);

	int majorver, minorver;
	majorver = read_uint16(conn);
	minorver = read_uint16(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	LOG(LOG_INFO, "recv ver = %d.%d", majorver, minorver);

	write_raw_string(conn, magicstr);
	write_uint16(conn, majorver);
	write_uint16(conn, minorver);
	write_string(conn, CONFIG_HOSTNAME);
	flush_resp(conn);

	return 0;
}

static int
proto_handle_qinf(struct synergy_proto_conn *conn)
{
	if (conn->recv_len != 0) {
		LOG(LOG_ERROR, "invalid pkt len (got %d bytes, expected %d)",
				conn->recv_len, 0);
		return 1;
	}

	uint16_t x = CONFIG_SCREENX, y = CONFIG_SCREENY;
	uint16_t w = CONFIG_SCREENW, h = CONFIG_SCREENH;
	uint16_t warp_size = 0;
	uint16_t mpos_x = 0, mpos_y = 0;
	write_raw_string(conn, "DINF");
	write_uint16(conn, x);
	write_uint16(conn, y);
	write_uint16(conn, w);
	write_uint16(conn, h);
	write_uint16(conn, warp_size);
	write_uint16(conn, mpos_x);
	write_uint16(conn, mpos_y);
	flush_resp(conn);

	return 0;
}

static int
proto_handle_dummy(struct synergy_proto_conn *conn, int exp_len)
{
	if (conn->recv_len != exp_len) {
		LOG(LOG_ERROR, "invalid pkt len (got %d bytes, expected %d)",
				conn->recv_len, exp_len);
		return 1;
	}

	return 0;
}

static int
proto_handle_set_options(struct synergy_proto_conn *conn)
{
	int num_opts;

	num_opts = read_uint32(conn);
	EXIT_ON_RECV_ERROR(conn);

	if (num_opts % 2 == 1) {
		LOG(LOG_ERROR, "invalid opts num - expecting even, got %d",
				num_opts);
		return 1;
	}

	if (num_opts * 4 != conn->recv_len) {
		LOG(LOG_ERROR, "invalid pkt len (got %d bytes, expected %d) "
				"(expecting %d opts)",
				conn->recv_len, num_opts * 8, num_opts);
		return 1;
	}

	while (num_opts > 0) {
		char *tag_str = conn->recv_buf;
		uint32_t tag = read_uint32(conn);
		(void)tag;
		uint32_t val = read_uint32(conn);
		LOG(LOG_INFO, "%.4s = %"PRIu32, tag_str, val);
		num_opts -= 2;
	}

	EXIT_ON_INVALID_RECV_PKT(conn);
	return 0;
}

static int
proto_handle_keepalive(struct synergy_proto_conn *conn)
{
	if (conn->recv_len != 0) {
		LOG(LOG_ERROR, "invalid pkt len (got %d bytes, expected %d)",
				conn->recv_len, 0);
		return 1;
	}

	write_raw_string(conn, "CALV");
	flush_resp(conn);

	return 0;
}

static bool g_skip_next_mouse_move = false;

static int
proto_handle_screen_enter(struct synergy_proto_conn *conn)
{
	uint16_t enter_x = read_uint16(conn);
	uint16_t enter_y = read_uint16(conn);
	uint32_t seq_no = read_uint32(conn);
	uint16_t key_mod_mask = read_uint16(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	LOG(LOG_INFO, "screen enter; x=%u, y=%u, seq_no=%u, key_mask=%u",
			enter_x, enter_y, seq_no, key_mod_mask);


	g_skip_next_mouse_move = true;
	serial_ard_set_mouse_pos(enter_x, enter_y);

	return 0;
}

static int
proto_handle_clipboard_sync(struct synergy_proto_conn *conn)
{
	uint8_t id = read_uint8(conn);
	uint32_t seq_id = read_uint32(conn);
	uint8_t unk = read_uint8(conn);
	uint32_t str_len = read_uint32(conn);
	const char *str = (const char *)conn->recv_buf;
	(void)id;
	(void)seq_id;
	(void)unk;
	(void)str;
	read_nbytes(conn, str_len);
	EXIT_ON_INVALID_RECV_PKT(conn);

	/* this can't work obviously */
	return 0;
}

static int
proto_handle_mouse_move(struct synergy_proto_conn *conn)
{
	uint16_t abs_x = read_uint16(conn);
	uint16_t abs_y = read_uint16(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	if (g_skip_next_mouse_move) {
		g_skip_next_mouse_move = false;
		return 0;
	}

	int16_t x_delta, y_delta;
	x_delta = abs_x - conn->mouse_x;
	y_delta = abs_y - conn->mouse_y;

	//LOG(LOG_INFO, "mouse move (delta %d,%d)", x_delta, y_delta);
	serial_ard_set_mouse_pos(abs_x, abs_y);

	conn->mouse_x = abs_x;
	conn->mouse_y = abs_y;

	return 0;
}

static int
proto_handle_rel_mouse_move(struct synergy_proto_conn *conn)
{
	int16_t x_delta = read_int16(conn);
	int16_t y_delta = read_int16(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	//LOG(LOG_INFO, "rel mouse move (%d,%d)", x_delta, y_delta);

	serial_ard_mouse_move(x_delta, y_delta);

	if (x_delta < conn->mouse_x) {
		x_delta = conn->mouse_x;
	}

	if (y_delta < conn->mouse_y) {
		y_delta = conn->mouse_y;
	}

	if (conn->mouse_x + x_delta >= CONFIG_SCREENW) {
		x_delta = CONFIG_SCREENW - conn->mouse_x - 1;
	}

	if (conn->mouse_x + x_delta >= CONFIG_SCREENH) {
		y_delta = CONFIG_SCREENH - conn->mouse_y - 1;
	}

	conn->mouse_x += x_delta;
	conn->mouse_y += y_delta;

	return 0;
}

static uint8_t
synergy_mouse_btn_to_arduino(uint8_t id)
{
	switch (id) {
		case 1: /* LMB */
			return 1 << 0;
		case 2: /* MMB */
			return 1 << 2;
		case 3: /* RMB */
			return 1 << 1;
		case 6: /* PREV */
			return 1 << 3;
		case 7: /* NEXT */
			return 1 << 4;
		default:
			break;
	}

	return 0;
}

static int
proto_handle_mouse_down(struct synergy_proto_conn *conn)
{
	uint8_t id = read_uint8(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	LOG(LOG_DEBUG_1, "mouse down (%d)", id);

	id = synergy_mouse_btn_to_arduino(id);
	serial_ard_mouse_down(id);
	return 0;
}

static int
proto_handle_mouse_up(struct synergy_proto_conn *conn)
{
	uint8_t id = read_uint8(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	LOG(LOG_DEBUG_1, "mouse up (%d)", id);

	id = synergy_mouse_btn_to_arduino(id);
	serial_ard_mouse_up(id);
	return 0;
}

#define SIGNUM(a) ((a) < 0 ? -1 : ((a) > 0 ? 1 : 0))
static int
proto_handle_mouse_wheel(struct synergy_proto_conn *conn)
{
	int16_t x_delta = read_int16(conn);
	int16_t y_delta = read_int16(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	LOG(LOG_DEBUG_1, "mouse wheel (%d,%d)", x_delta, y_delta);

	serial_ard_mouse_wheel(SIGNUM(x_delta), SIGNUM(y_delta));
	return 0;
}

/* 0xEFXX */
static uint8_t g_keycodes[] = {
	[0x08] = KEY_BACKSPACE,
	[0x09] = KEY_TAB,
	[0x0A] = KEY_ENTER,
	[0x0B] = KEY_CLEAR,
	[0x0D] = KEY_RETURN,
	[0x13] = KEY_PAUSE,
	[0x14] = KEY_SCROLL_LOCK,
	[0x15] = KEY_SYSREQ_ATTENTION,
	[0x1B] = KEY_ESC,
	[0xFF] = KEY_DELETE,
	[0x50] = KEY_HOME,
	[0x51] = KEY_LEFT_ARROW,
	[0x52] = KEY_UP_ARROW,
	[0x53] = KEY_RIGHT_ARROW,
	[0x54] = KEY_DOWN_ARROW,
	[0x55] = KEY_PAGE_UP,
	[0x56] = KEY_PAGE_DOWN,
	[0x57] = KEY_END,
	[0x58] = KEY_HOME,
	[0x60] = KEY_SELECT,
	[0x61] = KEY_PRINT,
	[0x62] = KEY_EXECUTE,
	[0x63] = KEY_INSERT,
	[0x65] = KEY_UNDO,
	[0x66] = KEY_AGAIN,
	[0x67] = KEY_MENU,
	[0x68] = KEY_FIND,
	[0x69] = KEY_CANCEL,
	[0x6A] = KEY_HELP,
	[0x6B] = KEY_STOP,
	[0x7E] = KEY_RIGHT_ALT,
	[0x7F] = KEY_NUM_LOCK,

	[0x8D] = KEYPAD_ENTER,
	[0x95] = KEY_HOME,
	[0x96] = KEY_LEFT,
	[0x97] = KEY_UP,
	[0x98] = KEY_RIGHT,
	[0x99] = KEY_DOWN,
	[0x9A] = KEY_PAGE_UP,
	[0x9B] = KEY_PAGE_DOWN,
	[0x9C] = KEY_END,
	[0x9D] = KEY_HOME,
	[0x9E] = KEY_INSERT,
	[0x9F] = KEY_DELETE,
	[0xBD] = KEYPAD_EQUAL_SIGN,
	[0xAA] = KEYPAD_MULTIPLY,
	[0xAB] = KEYPAD_ADD,
	[0xAD] = KEYPAD_SUBTRACT,
	[0xAE] = KEYPAD_COLON,
	[0xAF] = KEYPAD_DIVIDE,
	[0xB0] = KEYPAD_0,
	[0xB1] = KEYPAD_1,
	[0xB2] = KEYPAD_2,
	[0xB3] = KEYPAD_3,
	[0xB4] = KEYPAD_4,
	[0xB5] = KEYPAD_5,
	[0xB6] = KEYPAD_6,
	[0xB7] = KEYPAD_7,
	[0xB8] = KEYPAD_8,
	[0xB9] = KEYPAD_9,

	[0xBE] = KEY_F1,
	[0xBF] = KEY_F2,
	[0xC0] = KEY_F3,
	[0xC1] = KEY_F4,
	[0xC2] = KEY_F5,
	[0xC3] = KEY_F6,
	[0xC4] = KEY_F7,
	[0xC5] = KEY_F8,
	[0xC6] = KEY_F9,
	[0xC7] = KEY_F10,
	[0xC8] = KEY_F11,
	[0xC9] = KEY_F12,
	[0xCA] = KEY_F13,
	[0xCB] = KEY_F14,
	[0xCC] = KEY_F15,
	[0xCD] = KEY_F16,
	[0xCE] = KEY_F17,
	[0xCF] = KEY_F18,
	[0xD0] = KEY_F19,
	[0xD1] = KEY_F20,
	[0xD2] = KEY_F21,
	[0xD3] = KEY_F22,
	[0xD4] = KEY_F23,
	[0xD5] = KEY_F24,
	[0xD6] = 0, /* arduino can't submit F25-35 */
	[0xD7] = 0,
	[0xD8] = 0,
	[0xD9] = 0,
	[0xDA] = 0,
	[0xDB] = 0,
	[0xDC] = 0,
	[0xDD] = 0,
	[0xDE] = 0,
	[0xDF] = 0,
	[0xE0] = 0, /* F35 */
	[0xE1] = KEY_LEFT_SHIFT,
	[0xE2] = KEY_RIGHT_SHIFT,
	[0xE3] = KEY_LEFT_CTRL,
	[0xE4] = KEY_RIGHT_CTRL,
	[0xE5] = KEY_CAPS_LOCK,
	[0xE9] = KEY_LEFT_ALT,
	[0xEA] = KEY_RIGHT_ALT,
	[0xEB] = KEY_LEFT_WINDOWS,
	[0xEC] = KEY_RIGHT_WINDOWS,
};

/* 0xE0XX */
static uint16_t g_special_keymap[] = {
	[0x5F] = CONSUMER_SLEEP,
	[0xA6] = CONSUMER_BROWSER_BACK,
	[0xA7] = CONSUMER_BROWSER_FORWARD,
	[0xA8] = CONSUMER_BROWSER_REFRESH,
	[0xAB] = CONSUMER_BROWSER_BOOKMARKS,
	[0xAC] = CONSUMER_BROWSER_HOME,
	[0xAD] = MEDIA_VOLUME_MUTE,
	[0xAE] = MEDIA_VOLUME_DOWN,
	[0xAF] = MEDIA_VOLUME_UP,
	[0xB0] = MEDIA_NEXT,
	[0xB1] = MEDIA_PREVIOUS,
	[0xB2] = MEDIA_STOP,
	[0xB3] = MEDIA_PLAY_PAUSE,
	[0xB4] = CONSUMER_EMAIL_READER,
	[0xB5] = CONSUMER_CONTROL_CONFIGURATION,
	[0xB6] = CONSUMER_CALCULATOR,
	[0xB7] = CONSUMER_EXPLORER,
	[0xB8] = CONSUMER_BRIGHTNESS_DOWN,
	[0xB9] = CONSUMER_BRIGHTNESS_UP,
};

static uint8_t g_char_keymap[] = {
	['a'] = KEY_A,
	['A'] = KEY_A,
	['b'] = KEY_B,
	['B'] = KEY_B,
	['c'] = KEY_C,
	['C'] = KEY_C,
	['d'] = KEY_D,
	['D'] = KEY_D,
	['e'] = KEY_E,
	['E'] = KEY_E,
	['f'] = KEY_F,
	['F'] = KEY_F,
	['g'] = KEY_G,
	['G'] = KEY_G,
	['h'] = KEY_H,
	['H'] = KEY_H,
	['i'] = KEY_I,
	['I'] = KEY_I,
	['j'] = KEY_J,
	['J'] = KEY_J,
	['k'] = KEY_K,
	['K'] = KEY_K,
	['l'] = KEY_L,
	['L'] = KEY_L,
	['m'] = KEY_M,
	['M'] = KEY_M,
	['n'] = KEY_N,
	['N'] = KEY_N,
	['o'] = KEY_O,
	['O'] = KEY_O,
	['p'] = KEY_P,
	['P'] = KEY_P,
	['q'] = KEY_Q,
	['Q'] = KEY_Q,
	['r'] = KEY_R,
	['R'] = KEY_R,
	['s'] = KEY_S,
	['S'] = KEY_S,
	['t'] = KEY_T,
	['T'] = KEY_T,
	['u'] = KEY_U,
	['U'] = KEY_U,
	['v'] = KEY_V,
	['V'] = KEY_V,
	['w'] = KEY_W,
	['W'] = KEY_W,
	['x'] = KEY_X,
	['X'] = KEY_X,
	['y'] = KEY_Y,
	['Y'] = KEY_Y,
	['z'] = KEY_Z,
	['Z'] = KEY_Z,
	['0'] = KEY_0,
	[')'] = KEY_0,
	['1'] = KEY_1,
	['!'] = KEY_1,
	['2'] = KEY_2,
	['@'] = KEY_2,
	['3'] = KEY_3,
	['#'] = KEY_3,
	['4'] = KEY_4,
	['$'] = KEY_4,
	['5'] = KEY_5,
	['%'] = KEY_5,
	['6'] = KEY_6,
	['^'] = KEY_6,
	['7'] = KEY_7,
	['&'] = KEY_7,
	['8'] = KEY_8,
	['*'] = KEY_8,
	['9'] = KEY_9,
	['('] = KEY_9,
	['`'] = KEY_TILDE,
	['~'] = KEY_TILDE,
	[' '] = KEY_SPACE,
	['['] = KEY_LEFT_BRACE,
	['{'] = KEY_LEFT_BRACE,
	[']'] = KEY_RIGHT_BRACE,
	['}'] = KEY_RIGHT_BRACE,
	[';'] = KEY_SEMICOLON,
	[':'] = KEY_SEMICOLON,
	['\''] = KEY_QUOTE,
	['"'] = KEY_QUOTE,
	['\\'] = KEY_BACKSLASH,
	['|'] = KEY_BACKSLASH,
	[','] = KEY_COMMA,
	['.'] = KEY_PERIOD,
	['/'] = KEY_SLASH,
	['?'] = KEY_SLASH,
	['<'] = HID_KEYBOARD_COMMA_AND_LESS_THAN,
	['>'] = HID_KEYBOARD_PERIOD_AND_GREATER_THAN,
	['-'] = KEY_MINUS,
	['_'] = KEY_MINUS,
	['='] = KEY_EQUAL,
	['+'] = KEY_EQUAL,
};

static uint16_t
synergy_key_to_arduino(uint16_t s_id, uint16_t char_id)
{
	uint8_t prefix = s_id >> 8;

	if (prefix == 0) {
		s_id = char_id;
		prefix = s_id >> 8;
	}

	uint8_t off = s_id & 0xFF;
	if (prefix == 0xE0) {
		if (off < sizeof(g_special_keymap) / sizeof(g_special_keymap[0])) {
			return g_special_keymap[off];
		}
	} else if (prefix == 0xEF) {
		if (off < sizeof(g_keycodes) / sizeof(g_keycodes[0])) {
			return g_keycodes[off];
		}
	} else if (prefix == 0xEE) {
		if (off == 0x20) {
			return KEYPAD_TAB;
		}
	} else if (prefix == 0) {
		if (char_id < sizeof(g_char_keymap) / sizeof(g_char_keymap[0])) {
			return g_char_keymap[char_id];
		}
	}

	return s_id;
}

static int
proto_handle_key_down(struct synergy_proto_conn *conn)
{
	uint16_t id = read_uint16(conn);
	uint16_t mods = read_uint16(conn);
	uint16_t phys_id = read_uint16(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	uint16_t ard_id = synergy_key_to_arduino(phys_id, id);
	LOG(LOG_DEBUG_1, "key down (id=0x%x, phys_id=0x%x, mods=0x%.4x)", id, phys_id, mods);

	serial_ard_key_down(ard_id);
	return 0;
}

static int
proto_handle_key_up(struct synergy_proto_conn *conn)
{
	uint16_t id = read_uint16(conn);
	uint16_t mods = read_uint16(conn);
	uint16_t phys_id = read_uint16(conn);
	EXIT_ON_INVALID_RECV_PKT(conn);

	uint16_t ard_id = synergy_key_to_arduino(phys_id, id);
	LOG(LOG_DEBUG_1, "key up (id=0x%x, phys_id=0x%x, mods=0x%.4x)", id, phys_id, mods);

	serial_ard_key_up(ard_id);
	return 0;
}

int
synergy_handle_pkt(struct synergy_proto_conn *conn)
{
	uint32_t tag;

	assert(conn->recv_len >= 4);
	tag = read_uint32(conn);

	/* TODO binary search perhaps? */
	if (tag == STR2TAG("QINF")) {
		return proto_handle_qinf(conn);
	} else if (tag == STR2TAG("CIAK")) {
		return proto_handle_dummy(conn, 0);
	} else if (tag == STR2TAG("CROP")) {
		return proto_handle_dummy(conn, 0);
	} else if (tag == STR2TAG("DSOP")) {
		return proto_handle_set_options(conn);
	} else if (tag == STR2TAG("CALV")) {
		return proto_handle_keepalive(conn);
	} else if (tag == STR2TAG("CINN")) {
		return proto_handle_screen_enter(conn);
	} else if (tag == STR2TAG("DCLP")) {
		return proto_handle_clipboard_sync(conn);
	} else if (tag == STR2TAG("COUT")) {
		serial_ard_all_up();
		return proto_handle_dummy(conn, 0);
	} else if (tag == STR2TAG("DMMV")) {
		return proto_handle_mouse_move(conn);
	} else if (tag == STR2TAG("DMRM")) {
		return proto_handle_rel_mouse_move(conn);
	} else if (tag == STR2TAG("DMDN")) {
		return proto_handle_mouse_down(conn);
	} else if (tag == STR2TAG("DMUP")) {
		return proto_handle_mouse_up(conn);
	} else if (tag == STR2TAG("DMWM")) {
		return proto_handle_mouse_wheel(conn);
	} else if (tag == STR2TAG("DKDN")) {
		return proto_handle_key_down(conn);
	} else if (tag == STR2TAG("DKRP")) {
		return proto_handle_dummy(conn, 8);
	} else if (tag == STR2TAG("DKUP")) {
		return proto_handle_key_up(conn);
	}

	LOG(LOG_INFO, "unknown pkt: %.4s (%d)", conn->recv_buf - 4, conn->recv_len + 4);

	return 0;
}