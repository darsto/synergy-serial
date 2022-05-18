/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <getopt.h>
 #include <sys/timerfd.h>

#include "synergy_proto.h"
#include "common.h"
#include "serial.h"
#include "config.h"

static struct synergy_proto_conn g_conn = {};
static struct {
	const char *serial_devpath;
	int baudrate;
} g_args;

static struct option g_options[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "baudrate", required_argument, NULL, 'b' },
	{ "device", required_argument, NULL, 'd' },
	{ 0, 0, 0, 0 },
};

static void
print_help(const char *argv0)
{
	fprintf(stderr, "%s -d /path/to/serialdev -b baudrate\n", argv0);
}

int
main(int argc, char *argv[])
{
	int fd;
	struct sockaddr_in saddr_in = {};
	struct pkt_buf {
		char prev[2048];
		char cur[2048];
	} pkt_buf;
	int prevlen = 0;
	char *bufptr;
	int rc;
	unsigned buflen;
	int serialfd;
	int skip_nbytes = 0;

	while (1) {
		int opt_index = 0;
		char c;

		c = getopt_long(argc, argv, "hb:d:", g_options, &opt_index);
		if (c == -1) {
			break;
		}

		switch (c) {
			case 0: {
				struct option *opt = &g_options[opt_index];
				if (opt->flag != NULL) {
					break;
				}

				if (optarg) {
					// todo
				}
				break;
			}
			case 'h':
				print_help(argv[0]);
				return 0;
			case 'd':
				g_args.serial_devpath = optarg;
				break;
			case 'b':
				g_args.baudrate = atoi(optarg);
				break;
			case '?':
				break;
			default:
				break;
		}
	}

	if (!g_args.serial_devpath || !g_args.baudrate) {
		print_help(argv[0]);
		return 1;
	}

#define BAUDRATE(rate) case rate: g_args.baudrate = B ## rate; break

	switch (g_args.baudrate) {
		BAUDRATE(57600);
		BAUDRATE(115200);
		BAUDRATE(230400);
		BAUDRATE(460800);
		BAUDRATE(500000);
		BAUDRATE(576000);
		BAUDRATE(921600);
		BAUDRATE(1000000);
		BAUDRATE(1152000);
		BAUDRATE(2000000);
		BAUDRATE(2500000);
		BAUDRATE(3000000);
		BAUDRATE(3500000);
		BAUDRATE(4000000);
		default:
			g_args.baudrate = 0;
			break;
	}

#undef BAUDRATE

	if (!g_args.baudrate) {
		LOG(LOG_ERROR, "Invalid baudrate. Only a few are supported. See the code for details.");
		return 1;
	}

	serialfd = open(g_args.serial_devpath, O_RDWR | O_NOCTTY | O_SYNC);
    if (serialfd < 0) {
        LOG(LOG_ERROR, "Can't open serial device at \"%s\": %s\n",
				g_args.serial_devpath, strerror(errno));
        return 1;
    }

	serial_set_fd(serialfd, g_args.baudrate, 0, 1); /* given baudrate with 8n1 (no parity) */

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

	rc = recv(g_conn.fd, pkt_buf.cur, sizeof(pkt_buf.cur), 0);
	if (rc < 0) {
		LOG(LOG_ERROR, "recv: %d", rc);
		return 1;
	}

	if (rc < 4) {
		LOG(LOG_ERROR, "recv invalid packet, len=%d", rc);
		return 1;
	}

	uint32_t len = ntohl(*(uint32_t *)pkt_buf.cur);
	if (len + 4 != rc) {
		LOG(LOG_ERROR, "recv malformed/incomplete greeting packet");
		return 1;
	}

	g_conn.recv_buf = pkt_buf.cur + 4;
	g_conn.recv_len = len;
	rc = synergy_proto_handle_greeting(&g_conn);
	if (rc < 0) {
		LOG(LOG_ERROR, "synergy_proto_handle_greeting() returned %d", rc);
		return 1;
	}

	int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (timerfd < 0) {
		LOG(LOG_ERROR, "timerfd_create() returned %d", errno);
		return 1;
	}

	struct itimerspec timerfd_time = { 0 };
	timerfd_time.it_interval.tv_nsec = 1000 * CONFIG_SERIAL_MOUSE_INTERVAL_MS;
	timerfd_time.it_value.tv_nsec = 1000 * CONFIG_SERIAL_MOUSE_INTERVAL_MS;
	timerfd_settime(timerfd, 0, &timerfd_time, NULL);

	struct pollfd pfds[2];
	pfds[0].fd = g_conn.fd;
	pfds[0].events = POLLIN | POLLERR;

	pfds[1].fd = timerfd;
	pfds[1].events = POLLIN;

	while (1) {
		rc = poll(pfds, sizeof(pfds) / sizeof(pfds[0]), -1);
		if (rc < 0) {
			LOG(LOG_ERROR, "poll returned %d, errno=%d", rc, errno);
			return 1;
		}

		if (pfds[0].revents & POLLIN) {
			pfds[0].revents &= ~POLLIN;

			rc = recv(fd, pkt_buf.cur, sizeof(pkt_buf.cur), 0);
			if (rc < 0) {
				LOG(LOG_ERROR, "recv returned %d, errno=%d", rc, errno);
				return 1;
			}

			bufptr = pkt_buf.cur;
			buflen = rc;

			if (skip_nbytes > 0) {
				prevlen = 0;

				if (skip_nbytes >= buflen) {
					skip_nbytes -= buflen;
					continue;
				} else {
					bufptr += skip_nbytes;
					buflen -= skip_nbytes;
					skip_nbytes = 0;
				}
			}

			if (prevlen > 0) {
				bufptr = pkt_buf.prev + sizeof(pkt_buf.prev) - prevlen;
				buflen += prevlen;
				prevlen = 0;
			}

			while (buflen >= 0) {
				if (buflen < 4) {
					memcpy(pkt_buf.prev + sizeof(pkt_buf.prev) - buflen, bufptr, buflen);
					prevlen = buflen;
					break;
				}

				uint32_t len = ntohl(*(uint32_t *)bufptr);
				if (len + 4 >= 65536) {
					/* we certainly screwed up somewhere */
					LOG(LOG_ERROR, "recv incomplete packet: pktlen=%d, buflen=%d", len, rc);
					return 1;
				}

				if (len + 4 >= 2048) {
					/* we don't support packets this big (like clipboard contents) */
					skip_nbytes = len + 4 - buflen;
					LOG(LOG_ERROR, "recv too big packet: pktlen=%d, buflen=%d", len, rc);
					break;
				}

				if (len + 4 > buflen) {
					if (bufptr < pkt_buf.cur) {
						LOG(LOG_ERROR, "recv too fragmented packet. expected len=%d, two packets len=%d", len, buflen);
						return 1;
					}

					memcpy(pkt_buf.prev + sizeof(pkt_buf.prev) - buflen, bufptr, buflen);
					prevlen = buflen;
					break;
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

		if (pfds[1].revents & POLLIN) {
			pfds[0].revents &= ~POLLIN;

			serial_ard_kick_mouse_move();

			usleep(16 * 1000);
		}
	}

	return 0;
}
