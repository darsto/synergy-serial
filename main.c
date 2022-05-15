/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <errno.h>
#include <netinet/tcp.h>

#include "synergy_proto.h"
#include "common.h"
#include "serial.h"

struct synergy_proto_conn g_conn = {};

static int
open_serial(const char *devpath, int baudrate)
{
    int fd = open(devpath, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        int rc = errno;
        fprintf(stderr, "error %d opening %s: %s\n", errno, devpath, strerror(rc));
        return -rc;
    }

    serial_set_interface_attribs(fd, baudrate, 0);  // set speed to 115,200 bps, 8n1 (no parity)
    serial_set_blocking(fd, 1);                // set blocking

    return fd;
}

int
main(int argc, char *argv[])
{
	int fd;
	struct sockaddr_in saddr_in = {};
	char buf[4096];
	char *bufptr;
	int rc;
	unsigned buflen;

#if 0
	fd = open_serial("/dev/ttyUSB2", B115200);
	write(fd, "hello!\n", 7);  // send 7 character greeting
	usleep((7 + 25) * 100);	   // sleep enough to transmit the 7 plus

	return 0;
#endif

	// Create socket
	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (fd == -1) {
		LOG(LOG_ERROR, "Could not create socket");
		return 1;
	}

	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons(24800);
	inet_pton(AF_INET, "127.0.0.1", &saddr_in.sin_addr);

	rc = connect(fd, (struct sockaddr *)&saddr_in, sizeof(saddr_in));
	if (rc < 0) {
		LOG(LOG_ERROR, "connect: %s", strerror(errno));
		return 1;
	}

	g_conn.fd = fd;
	LOG(LOG_INFO, "connected");

	rc = recv(fd, buf, sizeof(buf), 0);
	if (rc < 0) {
		LOG(LOG_ERROR, "recv: %d", rc);
		return 1;
	}

	if (rc < 4) {
		LOG(LOG_ERROR, "recv invalid packet, len=%d", rc);
		return 1;
	}

	uint32_t len = ntohl(*(uint32_t *)buf);
	if (len + 4 != rc) {
		LOG(LOG_ERROR, "recv malformed/incomplete greeting packet");
		return 1;
	}

	g_conn.recv_buf = buf + 4;
	g_conn.recv_len = len;
	rc = synergy_proto_handle_greeting(&g_conn);
	if (rc < 0) {
		LOG(LOG_ERROR, "synergy_proto_handle_greeting() returned %d", rc);
		return 1;
	}

	struct pollfd pfds[1];
	pfds[0].fd = g_conn.fd;
	pfds[0].events = POLLIN | POLLERR;

	while (1) {
		rc = poll(pfds, sizeof(pfds) / sizeof(pfds[0]), -1);
		if (rc < 0) {
			LOG(LOG_ERROR, "poll returned %d, errno=%d", rc, errno);
			return 1;
		}

		if (pfds[0].revents & POLLIN) {
			pfds[0].revents &= ~POLLIN;

			rc = recv(fd, buf, sizeof(buf), 0);
			if (rc < 0) {
				LOG(LOG_ERROR, "recv returned %d, errno=%d", rc, errno);
				return 1;
			}

			bufptr = buf;
			buflen = rc;
			while (buflen >= 8) {
				uint32_t len = ntohl(*(uint32_t *)bufptr);
				if (len + 4 > buflen) {
					LOG(LOG_ERROR, "recv incomplete packet: pktlen=%d, buflen=%d", len, rc);
					return 1;
				}

				g_conn.recv_buf = bufptr + 4;
				g_conn.recv_len = len;
				rc = synergy_handle_pkt(&g_conn);
				if (rc < 0) {
					LOG(LOG_ERROR, "synergy_handle_pkt() returned %d", rc);
					return 1;
				}

				bufptr += len + 4;
				buflen -= len + 4;
			}
		}
	}

	return 0;
}
