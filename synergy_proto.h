/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

struct synergy_proto_conn {
    int fd;
};

int synergy_proto_handle_greeting(struct synergy_proto_conn *conn, char *buf, int len);