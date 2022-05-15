/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

struct synergy_proto_conn {
    int fd;
    char *recv_buf;
    int recv_len;
    char resp_buf[512];
    int resp_len;
    int recv_error; /**< non-zero on receive error */
};

int synergy_proto_handle_greeting(struct synergy_proto_conn *conn);
int synergy_handle_pkt(struct synergy_proto_conn *conn);