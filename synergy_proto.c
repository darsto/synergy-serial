/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "synergy_proto.h"
#include "common.h"

int
synergy_proto_handle_greeting(struct synergy_proto_conn *conn, char *buf, int len)
{
	const char *magicstr = "Synergy";

	if (len != strlen(magicstr) + 4) {
		LOG(LOG_ERROR, "invalid pkt len (got %d bytes, expected min %d)\n",
				len, strlen(magicstr) + 4);
		return 1;
	}

	if (strncmp(magicstr, buf, strlen(magicstr)) != 0) {
		fprintf(stderr, "recv greeting with wrong magic\n");
		return 1;
	}
	buf += strlen(magicstr);

	int majorver, minorver;
	majorver = htons(*(uint16_t *)buf);
	buf += 2;
	minorver = htons(*(uint16_t *)buf);
	buf += 2;

	LOG(LOG_INFO, "recv ver = %d.%d", majorver, minorver);
	return 0;
}