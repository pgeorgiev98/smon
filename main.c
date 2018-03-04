#include "system.h"
#include "cpu.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main()
{
	struct system_t system = system_init();

	// Just show the CPU frequencies
	int line_len[system.cpu_count];
	for (;;) {
		system_refresh_info(&system);
		for (int i = 0; i < system.cpu_count; ++i) {
			int l = printf("CPU %d : %d MHz",
					system.cpus[i].id,
					system.cpus[i].cur_freq / 1000);

			for (int i = l; i < line_len[i]; ++i)
				printf(" ");
			printf("\n");
			line_len[i] = l;
		}

		printf("\e[4A");
		sleep(1);
	}

	system_delete(system);
	return 0;
}
