#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include "logger.h"

#include "system.h"
#include "util.h"
#include "cpu.h"
#include "disk.h"
#include "interface.h"
#include "battery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>

#define error(...) { fprintf(stderr, __VA_ARGS__); exit(-1); }

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


volatile sig_atomic_t must_exit = 0;

void signal_handler(int signum)
{
    must_exit = 1;
}

int main(int argc, char **argv)
{
	// Catch SIGTERM
	{
		struct sigaction action;
		memset(&action, 0, sizeof(struct sigaction));
		action.sa_handler = signal_handler;
		sigaction(SIGTERM, &action, NULL);
	}

	struct system_t system = system_init();

	struct logger_t logger;
	struct logger_stat_t log_stats[128];
	int log_stats_count = 0;
	const char *log_filename = NULL;

	// Parse command line arguments
	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
			printf(
					"-h --help                            Print this help message\n"
					"-l --log filename stat0 stat1 ...    Log stats to csv file, where each stat can be:\n"
					"    cpuX{usage,temp,freq}\n"
					"    disk_NAME_{read,write}\n"
					"    iface_NAME_{read,write}\n"
					"    battery_NAME_{charge,current,voltage}\n");
			return 0;
		} else if (!strcmp(arg, "-l") || !strcmp(arg, "--log")) {
			++i;
			if (i == argc)
				error("Log file name required\n");
			log_filename = argv[i++];
			while (i < argc) {
				const char *stat_name = argv[i];
				struct logger_stat_t stat;
				int ok = 1;
				if (!strncmp(stat_name, "cpu", 3)) {
					char *id_end;
					int id = strtol(stat_name + 3, &id_end, 10);
					if (id_end != stat_name + 3) {
						stat.data.cpu_id = id;
						if (!strcmp(id_end, "u") || !strcmp(id_end, "usage"))
							stat.type = LOGGER_CPU_USAGE;
						else if (!strcmp(id_end, "t") || !strcmp(id_end, "temp"))
							stat.type = LOGGER_CPU_TEMPERATURE;
						else if (!strcmp(id_end, "f") || !strcmp(id_end, "freq"))
							stat.type = LOGGER_CPU_FREQUENCY;
						else
							ok = 0;
						if (ok)
							log_stats[log_stats_count++] = stat;
					}
				} else if (!strncmp(stat_name, "disk_", 5)) {
					const char *name_end = stat_name + 5;
					while (*name_end && *name_end != '_')
						++name_end;
					if (*name_end == '\0') {
						ok = 0;
					} else {
						int len = name_end - stat_name - 5;
						strncpy(stat.data.disk_name, stat_name + 5, len);
						stat.data.disk_name[len] = '\0';
						++name_end;
						if (!strcmp(name_end, "r") || !strcmp(name_end, "read"))
							stat.type = LOGGER_DISK_READ;
						else if (!strcmp(name_end, "w") || !strcmp(name_end, "write"))
							stat.type = LOGGER_DISK_WRITE;
						else
							ok = 0;
						if (ok)
							log_stats[log_stats_count++] = stat;
					}
				} else if (!strncmp(stat_name, "iface_", 6)) {
					const char *name_end = stat_name + 6;
					while (*name_end && *name_end != '_')
						++name_end;
					if (*name_end == '\0') {
						ok = 0;
					} else {
						int len = name_end - stat_name - 6;
						strncpy(stat.data.iface_name, stat_name + 6, len);
						stat.data.iface_name[len] = '\0';
						++name_end;
						if (!strcmp(name_end, "r") || !strcmp(name_end, "read"))
							stat.type = LOGGER_IFACE_READ;
						else if (!strcmp(name_end, "w") || !strcmp(name_end, "write"))
							stat.type = LOGGER_IFACE_WRITE;
						else
							ok = 0;
						if (ok)
							log_stats[log_stats_count++] = stat;
					}
				} else if (!strncmp(stat_name, "battery_", 8)) {
					const char *name_end = stat_name + 8;
					while (*name_end && *name_end != '_')
						++name_end;
					if (*name_end == '\0') {
						ok = 0;
					} else {
						int len = name_end - stat_name - 8;
						strncpy(stat.data.battery_name, stat_name + 8, len);
						stat.data.battery_name[len] = '\0';
						++name_end;
						if (!strcmp(name_end, "c") || !strcmp(name_end, "charge"))
							stat.type = LOGGER_BAT_CHARGE;
						else if (!strcmp(name_end, "cu") || !strcmp(name_end, "current"))
							stat.type = LOGGER_BAT_CURRENT;
						else if (!strcmp(name_end, "v") || !strcmp(name_end, "voltage"))
							stat.type = LOGGER_BAT_VOLTAGE;
						else
							ok = 0;
						if (ok)
							log_stats[log_stats_count++] = stat;
					}
				} else
					ok = 0;

				if (!ok) {
					--i;
					break;
				}
				++i;
			}
		} else {
			error("Unknown argument %s. Try %s --help\n", argv[i], argv[0]);
		}
	}

	int logger_ret = logger_init(&logger, CSV, log_filename, log_stats_count,
			log_stats);
	if (logger_ret != 0) {
		fprintf(stderr, "Failed to initialize logger: %d\n", logger_ret);
		return 1;
	}

	// Loop forever, show CPU usage and frequency and disk usage
	printf(TERM_CLEAR_SCREEN TERM_POSITION_HOME);
	for (;;) {
		system_refresh_info(&system);
		logger_log(&logger, &system);

		// CPU frequency and usage
		for (int c = 0; c < system.cpu_count; ++c) {
			const struct cpu_t *cpu = &system.cpus[c];
			if (c == 0 || cpu->core_id != system.cpus[c - 1].core_id) {
				// CPU info with temperature
				printf("CPU %d : %4d MHz %3d%% usage %3dC"
						TERM_ERASE_REST_OF_LINE "\n",
						c + 1,
						cpu->cur_freq / 1000,
						(int)(cpu->total_usage * 100),
						cpu->cur_temp / 1000);
			} else {
				// CPU info without temperature
				printf("CPU %d : %4d MHz %3d%% usage"
						TERM_ERASE_REST_OF_LINE "\n",
						c + 1,
						cpu->cur_freq / 1000,
						(int)(cpu->total_usage * 100));
			}
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

		if (system.battery_count > 0) {
			printf(TERM_ERASE_REST_OF_LINE "\n");
			// Battery info
			printf("Battery   Charge Current Voltage\n");
			for (int i = 0; i < system.battery_count; ++i) {
				const struct battery_t *battery = &system.batteries[i];
				printf("%-8s %6d%% %6.2fA %6.2fV" TERM_ERASE_REST_OF_LINE "\n",
						battery->name, battery->charge,
						battery->current / 1000000.f, battery->voltage / 1000000.f);
			}
		}

		printf(TERM_ERASE_REST_OF_LINE
				TERM_ERASE_DOWN
				TERM_POSITION_HOME);
		fflush(stdout);

		int c = wait_for_keypress();
		if (c == 'q' || c == 'Q' || c == 3 || must_exit)
			break;
	}

	logger_destroy(&logger);

	system_delete(system);
	return 0;
}
