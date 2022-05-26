#ifndef SYNERGY_SERIAL
#define SYNERGY_SERIAL

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdint.h>
#include <inttypes.h>

void serial_set_fd(int fd, int speed, int parity, int should_block);

int serial_ard_set_mouse_pos(uint16_t x, uint16_t y);
int serial_ard_mouse_move(int16_t x_delta, int16_t y_delta);
int serial_ard_kick_mouse_move(void);
int serial_ard_mouse_down(uint8_t id);
int serial_ard_mouse_up(uint8_t id);
int serial_ard_mouse_wheel(int16_t x_delta, int16_t y_delta);
int serial_ard_key_down(uint16_t id);
int serial_ard_key_up(uint16_t id);
int serial_ard_all_up(void);

#endif /* SYNERGY_SERIAL */