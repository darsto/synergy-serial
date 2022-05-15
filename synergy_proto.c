/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>

#include "synergy_proto.h"
#include "common.h"

#define STR2TAG(str) \
	((uint32_t)(((str)[0] << 24) | ((str)[1] << 16) | ((str)[2] << 8) | ((str)[3])))

static uint8_t
read_uint8(struct synergy_proto_conn *conn)
{
	if (conn->recv_len < 1) {
		conn->recv_error = -EINVAL;
		return 0;
	}

	conn->recv_buf += 1;
	conn->recv_len -= 1;
	return *(uint32_t *)(*conn->recv_buf - 1);
}

static int8_t
read_int8(struct synergy_proto_conn *conn)
{
	return (int8_t)read_uint8(conn);
}

static void
write_uint8(struct synergy_proto_conn *conn, uint8_t val)
{
	void *buf;

	assert(conn->resp_len + 1 < sizeof(conn->resp_buf));
	buf = conn->resp_buf + conn->resp_len;
	*(uint8_t *)buf = val;
	conn->resp_len += 1;
}

static void
write_int8(struct synergy_proto_conn *conn, int8_t val)
{
	write_uint8(conn, (uint8_t)val);
}

static uint16_t
read_uint16(struct synergy_proto_conn *conn)
{
	if (conn->recv_len < 2) {
		conn->recv_error = -EINVAL;
		return 0;

	}

	conn->recv_buf += 2;
	conn->recv_len -= 2;
	return *(uint32_t *)(*conn->recv_buf - 2);
}

static int16_t
read_int16(struct synergy_proto_conn *conn)
{
	return (int16_t)read_uint16(conn);
}

static void
write_uint16(struct synergy_proto_conn *conn, uint16_t val)
{
	void *buf;

	assert(conn->resp_len + 2 < sizeof(conn->resp_buf));
	buf = conn->resp_buf + conn->resp_len;
	*(uint16_t *)buf = htons(val);
	conn->resp_len += 2;
}

static void
write_int16(struct synergy_proto_conn *conn, int16_t val)
{
	write_uint16(conn, (uint16_t)val);
}

static uint32_t
read_uint32(struct synergy_proto_conn *conn)
{
	if (conn->recv_len < 4) {
		conn->recv_error = -EINVAL;
		return 0;
	}

	conn->recv_buf += 4;
	conn->recv_len -= 4;
	return *(uint32_t *)(*conn->recv_buf - 4);
}

static int32_t
read_int32(struct synergy_proto_conn *conn)
{
	return (int32_t)read_uint32(conn);
}

static void
write_uint32(struct synergy_proto_conn *conn, uint32_t val)
{
	void *buf;

	assert(conn->resp_len + 4 < sizeof(conn->resp_buf));
	buf = conn->resp_buf + conn->resp_len;
	*(uint32_t *)buf = htonl(val);
	conn->resp_len += 4;
}

static void
write_int32(struct synergy_proto_conn *conn, int32_t val)
{
	write_uint32(conn, (uint32_t)val);
}

static void
write_raw_string(struct synergy_proto_conn *conn, const char *str)
{
	void *buf;
	uint32_t len = strlen(str);

	assert(conn->resp_len + len < sizeof(conn->resp_buf));
	buf = conn->resp_buf + conn->resp_len;
	memcpy(buf, str, len);
	conn->resp_len += len;
}

static void
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

static void
clear_resp(struct synergy_proto_conn *conn)
{
	conn->resp_len = 0;
}

static void
flush_resp(struct synergy_proto_conn *conn)
{
	int rc;

	rc = send(conn->fd, conn->resp_buf, conn->resp_len, 0);
	if (rc < 0) {
		perror("send");
		return;
	}
	conn->resp_len = 0;
}

#define EXIT_ON_RECV_ERROR(conn) \
({ \
	if ((conn)->recv_error) { \
			LOG(LOG_ERROR, "recv error: %d\n", (conn)->recv_error); \
			clear_resp((conn)); \
		return -1; \
	} \
	if ((conn)->recv_len != 0) { \
			LOG(LOG_ERROR, "recv packet too long: %d bytes remaingin\n", (conn)->recv_len); \
			clear_resp((conn)); \
		return -1; \
	} \
})

int
synergy_proto_handle_greeting(struct synergy_proto_conn *conn)
{
	const char *magicstr = "Synergy";

	if (conn->recv_len != strlen(magicstr) + 4) {
		LOG(LOG_ERROR, "invalid pkt len (got %d bytes, expected %d)\n",
				conn->recv_len, strlen(magicstr) + 4);
		return -1;
	}

	if (strncmp(magicstr, conn->recv_buf, strlen(magicstr)) != 0) {
		fprintf(stderr, "recv greeting with wrong magic\n");
		return -1;
	}
	conn->recv_len += strlen(magicstr);

	int majorver, minorver;
	majorver = read_uint16(conn);
	minorver = read_uint16(conn);
	EXIT_ON_RECV_ERROR(conn);

	LOG(LOG_INFO, "recv ver = %d.%d", majorver, minorver);

	write_raw_string(conn, magicstr);
	write_uint16(conn, majorver);
	write_uint16(conn, minorver);
	write_string(conn, "mymachine");
	flush_resp(conn);

	return 0;
}

static int
proto_handle_qinf(struct synergy_proto_conn *conn)
{
	if (conn->recv_len != 0) {
		LOG(LOG_ERROR, "invalid pkt len (got %d bytes, expected %d)\n",
				conn->recv_len, 0);
		return 1;
	}

	uint16_t x, y, w, h;
	uint16_t warp_size;
	uint16_t mpos_x, mpos_y;
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

int
synergy_handle_pkt(struct synergy_proto_conn *conn)
{
	uint32_t tag;

	tag = read_uint32(conn);
	if (conn->recv_error) {
		LOG(LOG_ERROR, "recv error: %d\n", (conn)->recv_error);
		return -1;
	}

	if (tag == STR2TAG("QINF")) {
		return proto_handle_qinf(conn);
	}
}