#ifndef SYNERGY_SERIAL
#define SYNERGY_SERIAL

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdint.h>
#include <inttypes.h>

int serial_set_interface_attribs(int fd, int speed, int parity);
void serial_set_blocking(int fd, int should_block);

int serial_ard_set_mouse_pos(int fd, uint16_t x, uint16_t y);
int serial_ard_mouse_move(int fd, uint16_t x_delta, uint16_t y_delta);
int serial_ard_mouse_down(int fd, uint8_t id);
int serial_ard_mouse_up(int fd, uint8_t id);
int serial_ard_mouse_wheel(int fd, uint16_t x_delta, uint16_t y_delta);
int serial_ard_key_down(int fd, uint16_t id, uint16_t mods, uint16_t phys_id);
int serial_ard_key_up(int fd, uint16_t id, uint16_t mods, uint16_t phys_id);

#endif /* SYNERGY_SERIAL */