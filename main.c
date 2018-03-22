#include "system.h"
#include "util.h"
#include "cpu.h"
#include "disk.h"
#include "interface.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TERM_CLEAR_SCREEN "\e[2J"
#define TERM_POSITION_HOME "\e[H"
#define TERM_ERASE_REST_OF_LINE "\e[K"
#define TERM_ERASE_DOWN "\e[J"

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
					cpu->id,
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

		sleep(1);
	}

	system_delete(system);
	return 0;
}
