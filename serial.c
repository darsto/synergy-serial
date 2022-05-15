#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "serial.h"

static int g_fd;

void
serial_set_fd(int fd)
{
	g_fd = fd;
}

int
serial_set_interface_attribs(int speed, int parity)
{
	struct termios tty;
	if (tcgetattr(g_fd, &tty) != 0) {
		perror("tcgetattr");
		return -1;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;	 // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;	 // disable break processing
	tty.c_lflag = 0;		 // no signaling chars, no echo,
							 // no canonical processing
	tty.c_oflag = 0;		 // no remapping, no delays
	tty.c_cc[VMIN] = 0;		 // read doesn't block
	tty.c_cc[VTIME] = 5;	 // 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);	 // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);	// ignore modem controls,
										// enable reading
	tty.c_cflag &= ~(PARENB | PARODD);	// shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(g_fd, TCSANOW, &tty) != 0) {
		perror("tcsetattr");
		return -1;
	}
	return 0;
}

void
serial_set_blocking(int should_block)
{
	struct termios tty;
	memset(&tty, 0, sizeof tty);
	if (tcgetattr(g_fd, &tty) != 0) {
		perror("tcgetattr");
		return;
	}

	tty.c_cc[VMIN] = should_block ? 1 : 0;
	tty.c_cc[VTIME] = 5;  // 0.5 seconds read timeout

	if (tcsetattr(g_fd, TCSANOW, &tty) != 0) perror("tcsetattr");
}

struct serial_msg {
	char tag[4];
	uint16_t arg1;
	uint16_t arg2;
	uint16_t arg3;
	uint16_t arg4;
};

static int
serial_sendmsg(struct serial_msg *msg)
{
	return write(g_fd, msg, sizeof(*msg));
}

int
serial_ard_set_mouse_pos(uint16_t x, uint16_t y)
{
	serial_sendmsg(&(struct serial_msg){ "MSET", x, y });
}

int
serial_ard_mouse_move(int16_t x_delta, int16_t y_delta)
{
	serial_sendmsg(&(struct serial_msg){ "MMOV", x_delta, y_delta });
}

int
serial_ard_mouse_down(uint8_t id)
{
	serial_sendmsg(&(struct serial_msg){ "MBDN", id });
}

int
serial_ard_mouse_up(uint8_t id)
{
	serial_sendmsg(&(struct serial_msg){ "MBUP", id });
}

int
serial_ard_mouse_wheel(int16_t x_delta, int16_t y_delta)
{
	serial_sendmsg(&(struct serial_msg){ "MWHL", x_delta, y_delta });
}

int serial_ard_key_down(uint16_t id, uint16_t mods, uint16_t phys_id)
{
	serial_sendmsg(&(struct serial_msg){ "KBDN", id, mods, phys_id });
}

int serial_ard_key_up(uint16_t id, uint16_t mods, uint16_t phys_id)
{
	serial_sendmsg(&(struct serial_msg){ "KBUP", id, mods, phys_id });
}
