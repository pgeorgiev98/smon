#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include "system.h"
#include "util.h"
#include "cpu.h"
#include "disk.h"
#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

#define TERM_CLEAR_SCREEN "\e[2J"
#define TERM_POSITION_HOME "\e[H"
#define TERM_ERASE_REST_OF_LINE "\e[K"
#define TERM_ERASE_DOWN "\e[J"

static struct termios orig_termios;

static void reset_terminal_mode(void)
{
	tcsetattr(0, TCSANOW, &orig_termios);
}

static void set_conio_terminal_mode(void)
{
	struct termios new_termios;

	// take two copies - one for now, one for later
	tcgetattr(0, &orig_termios);
	memcpy(&new_termios, &orig_termios, sizeof(new_termios));

	// register cleanup handler, and set the new terminal mode
	atexit(reset_terminal_mode);
	cfmakeraw(&new_termios);
	tcsetattr(0, TCSANOW, &new_termios);
}

static int getch(void)
{
	int r;
	unsigned char c;
	if ((r = read(STDIN_FILENO, &c, sizeof(c))) < 0)
		return r;
	return c;
}

static int wait_for_keypress(void)
{
	set_conio_terminal_mode();
	// Set timeout to 1.0 seconds
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	// Initialize file descriptor sets
	fd_set read_fds, write_fds, except_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&except_fds);
	FD_SET(STDIN_FILENO, &read_fds);

	int c = -1;
	if (select(STDIN_FILENO + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1)
		c = getch();
	reset_terminal_mode();
	return c;
}


int main()
{
	struct system_t system = system_init();

	// Loop forever, show CPU usage and frequency and disk usage
	printf(TERM_CLEAR_SCREEN TERM_POSITION_HOME);
	for (;;) {
		system_refresh_info(&system);

		// CPU frequency and usage
		for (int c = 0; c < system.cpu_count; ++c) {
			const struct cpu_t *cpu = &system.cpus[c];
			printf("CPU %d : %4d MHz %3d%% usage"
					TERM_ERASE_REST_OF_LINE "\n",
					c + 1,
					cpu->cur_freq / 1000,
					(int)(cpu->total_usage * 100));
		}
		printf(TERM_ERASE_REST_OF_LINE "\n");

		// Disk usage
		printf("Disk            Read       Write\n");
		for (int d = 0; d < system.disk_count; ++d) {
			const struct disk_t *disk = &system.disks[d];
			char read[10], write[10];
			// TODO: find a way to check actual sector size
			bytes_to_human_readable(
					disk->stats_delta[DISK_READ_SECTORS] * 512, read);
			bytes_to_human_readable(
					disk->stats_delta[DISK_WRITE_SECTORS] * 512, write);

			printf("%-8s %9s/s %9s/s" TERM_ERASE_REST_OF_LINE "\n",
					disk->name, read, write);
		}

		printf(TERM_ERASE_REST_OF_LINE "\n");
		// Network usage
		printf("Interface   Download      Upload\n");
		for (int i = 0; i < system.interface_count; ++i) {
			const struct interface_t *interface = &system.interfaces[i];
			char down[10], up[10];
			bytes_to_human_readable(interface->delta_rx_bytes, down);
			bytes_to_human_readable(interface->delta_tx_bytes, up);
			printf("%-8s %9s/s %9s/s" TERM_ERASE_REST_OF_LINE "\n",
					interface->name, down, up);
		}

		printf(TERM_ERASE_REST_OF_LINE
				TERM_ERASE_DOWN
				TERM_POSITION_HOME);
		fflush(stdout);

		int c = wait_for_keypress();
		if (c == 'q' || c == 'Q' || c == 3)
			break;
	}

	system_delete(system);
	return 0;
}
