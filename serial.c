#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "serial.h"
#include "config.h"
#include "common.h"

static int g_fd = -1;
static int g_tx_freebufs = CONFIG_SERIAL_TX_SIZE;

static int
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
	tty.c_iflag &= ~(IGNBRK | IGNCR | ICRNL | ICRNL | IUCLC);	 // disable break processing
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

struct serial_msg {
	uint8_t tag[4];
	uint16_t arg1;
	uint16_t arg2;
};

static int serial_sendmsg(struct serial_msg *msg);

static void
get_free_tx_buf(void)
{
	uint8_t rx[CONFIG_SERIAL_TX_SIZE];
	int i, rc = 0;

	if (g_tx_freebufs > 0) {
		g_tx_freebufs--;
		return;
	}

	while (rc == 0) {
		rc = read(g_fd, &rx, sizeof(rx));
		if (rc < 0) {
			LOG(LOG_ERROR, "read() returned: %s", strerror(errno));
			return;
		}
	}

	for (i = 0; i < rc; i++) {
		if (rx[i] == 0xFF) {
			g_tx_freebufs = CONFIG_SERIAL_TX_SIZE - 1;
			serial_sendmsg(&(struct serial_msg){ "SCFG", CONFIG_SCREENW, CONFIG_SCREENH });
			return;
		} else if (rx[i] != 0x01) {
			LOG(LOG_ERROR, "read() returned non-1: %d", rx[i]);
		}
	}

	g_tx_freebufs += rc - 1;
}

static int
serial_sendmsg(struct serial_msg *msg)
{
	int8_t rc;

	get_free_tx_buf();

	rc = write(g_fd, (char *)msg, sizeof(*msg));
	if (rc < 0) {
		return -errno;
	}

	usleep(1600);

	return 0;
}

void
serial_set_fd(int fd, int speed, int parity, int should_block)
{
	g_fd = fd;
	serial_set_interface_attribs(speed, parity);
	serial_sendmsg(&(struct serial_msg){ "SCFG", CONFIG_SCREENW, CONFIG_SCREENH });
}

int
serial_ard_set_mouse_pos(uint16_t x, uint16_t y)
{
	return serial_sendmsg(&(struct serial_msg){ "MSET", x, y });
}

int
serial_ard_mouse_move(int16_t x_delta, int16_t y_delta)
{
	return serial_sendmsg(&(struct serial_msg){ "MMOV", x_delta, y_delta });
}

int
serial_ard_mouse_down(uint8_t id)
{
	return serial_sendmsg(&(struct serial_msg){ "MBDN", id });
}

int
serial_ard_mouse_up(uint8_t id)
{
	return serial_sendmsg(&(struct serial_msg){ "MBUP", id });
}

int
serial_ard_mouse_wheel(int16_t x_delta, int16_t y_delta)
{
	return serial_sendmsg(&(struct serial_msg){ "MWHL", x_delta, y_delta });
}

int serial_ard_key_down(uint16_t id, uint16_t mods, uint16_t phys_id)
{
	return serial_sendmsg(&(struct serial_msg){ "KBDN", id, mods });
}

int serial_ard_key_up(uint16_t id, uint16_t mods, uint16_t phys_id)
{
	return serial_sendmsg(&(struct serial_msg){ "KBUP", id, mods });
}
