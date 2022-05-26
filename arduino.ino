/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#define SERIAL_RX_BUFFER_SIZE 16 * 8
#define SERIAL_TX_BUFFER_SIZE 16 * 8

#include "HID-Project.h"

struct serial_msg {
	uint8_t tag[4];
	uint16_t arg1;
	uint16_t arg2;
};

#define STR2TAG(str) *(uint32_t *)(str)

void setup() {
  Serial1.begin(115200);

  /* dummy resolution -> we'll set up the proper one later */
  AbsoluteMouse.begin(1920, 1080);
  Keyboard.begin();

  /* let them know we've reset / powered-on */
  Serial1.write((uint8_t)0xFF);
}

struct serial_msg *
read_msg(void)
{
  static struct serial_msg msg;
  int off;

  if (Serial1.available() < sizeof(msg)) {
    return NULL;
  }

  off = 0;
  while (off < sizeof(msg)) {
    *(uint8_t *)((char *)(&msg) + off++) = Serial1.read();
  }

  /* let them know we've consumed a packet and they can send a new one */
  Serial1.write((uint8_t)0x1);

  return &msg;
}

void loop() {
  struct serial_msg *msg;
  uint32_t tag;

  msg = read_msg();
  if (!msg) {
    return;
  }

  tag = STR2TAG(msg->tag);

  if (tag == STR2TAG("SCFG")) {
    AbsoluteMouse.begin(msg->arg1, msg->arg2);
  } else if (tag == STR2TAG("MMOV")) {
    AbsoluteMouse.move((int16_t)msg->arg1, (int16_t)msg->arg2);
  } else if (tag == STR2TAG("MSET")) {
    AbsoluteMouse.moveTo(msg->arg1, msg->arg2, 0);
  } else if (tag == STR2TAG("MBDN")) {
    AbsoluteMouse.press(msg->arg1);
  } else if (tag == STR2TAG("MBUP")) {
    AbsoluteMouse.release(msg->arg1);
  } else if (tag == STR2TAG("MWHL")) {
    AbsoluteMouse.move(0, 0, (int16_t)msg->arg2);
  } else if (tag == STR2TAG("KBDN")) {
    Keyboard.press(KeyboardKeycode(msg->arg1));
  } else if (tag == STR2TAG("KBUP")) {
    Keyboard.release(KeyboardKeycode(msg->arg1));
  } else if (tag == STR2TAG("LEAV")) {
    AbsoluteMouse.release(0xFF);
    Keyboard.releaseAll();
  }
}
