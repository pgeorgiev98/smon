#include "system.h"
#include "cpu.h"
#include "disk.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TERM_CLEAR_SCREEN "\e[2J"
#define TERM_POSITION_HOME "\e[H"
#define TERM_ERASE_REST_OF_LINE "\e[K"
#define TERM_ERASE_DOWN "\e[J"

static void pretty_size(unsigned long long bytes, char *out)
{
	static const char * const units[] = {
		"B", "KiB", "MiB", "GiB", "TiB",
		"PiB", "EiB", "ZiB", "YiB"
	};
	static int units_count = sizeof(units) / sizeof(char *);

	int u, d = 0;
	for (u = 0; u < units_count && bytes >= 1024; ++u) {
		d = ((bytes % 1024) / 1024.0f) * 10;
		bytes /= 1024;
	}
	sprintf(out, "%llu.%d%s", bytes, d, units[u]);
}

int main()
{
	struct system_t system = system_init();

	// Loop forever, show CPU usage and frequency and disk usage
	printf(TERM_CLEAR_SCREEN TERM_POSITION_HOME);
	for (;;) {
		system_refresh_info(&system);
		int i = 0;
		for (int c = 0; c < system.cpu_count; ++c, ++i) {
			const struct cpu_t *cpu = &system.cpus[c];
			printf("CPU %d : %4d MHz %3d%% usage"
					TERM_ERASE_REST_OF_LINE "\n",
					cpu->id,
					cpu->cur_freq / 1000,
					(int)(cpu->total_usage * 100));
		}
		printf("\nDisk         Read       Write\n");
		for (int d = 0; d < system.disk_count; ++d, ++i) {
			const struct disk_t *disk = &system.disks[d];
			char read[10], write[10];
			pretty_size(disk->stats_delta[READ_SECTORS] * 512, read);
			pretty_size(disk->stats_delta[WRITE_SECTORS] * 512, write);

			printf("%-5s %9s/s %9s/s" TERM_ERASE_REST_OF_LINE "\n",
					disk->name, read, write);
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
