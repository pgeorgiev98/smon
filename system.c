#include "system.h"
#include "cpu.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

#define CPU_DEVICES_DIR "/sys/bus/cpu/devices/"
const int cpu_devices_dir_len = strlen(CPU_DEVICES_DIR);


static void system_cpu_init(struct system_t *);

struct system_t system_init()
{
	struct system_t system;

	system_cpu_init(&system);

	return system;
}

void system_delete(struct system_t system)
{
	free(system.cpus);
}



// Initialize the CPU portion of system
static void system_cpu_init(struct system_t *system)
{
	system->cpu_count = 0;
	system->cpus = NULL;
	int cpus_container_size = 0;

	// List /sys/bus/cpu/devices/
	char fname[128];
	strcpy(fname, CPU_DEVICES_DIR);
	DIR *cpu_devices_dir = opendir(CPU_DEVICES_DIR);
	struct dirent *cpu_ent;
	while ((cpu_ent = readdir(cpu_devices_dir))) {
		// Skip entities not starting with "cpu"
		if (strncmp(cpu_ent->d_name, "cpu", 3))
			continue;

		struct cpu_t cpu;
		cpu.id = atoi(cpu_ent->d_name + 3);

		// Set fname to /sys/bus/cpu/devices/cpuN
		strcpy(fname + cpu_devices_dir_len, cpu_ent->d_name);
		int cpu_dir_len = cpu_devices_dir_len +
			strlen(cpu_ent->d_name);
		
		strcpy(fname + cpu_dir_len, "/topology/core_id");
		cpu.core_id = read_int_from_file(fname);

		strcpy(fname + cpu_dir_len, "/topology/physical_package_id");
		cpu.package_id = read_int_from_file(fname);

		// Add cpu to system.cpus
		if (cpus_container_size <= system->cpu_count) {
			cpus_container_size += 64;
			system->cpus = (struct cpu_t *)realloc(system->cpus,
					sizeof(struct cpu_t) * cpus_container_size);
		}
		system->cpus[system->cpu_count++] = cpu;
	}
	closedir(cpu_devices_dir);

	// Sort the cpus array by package and core IDs
	int cmp(const void *a, const void *b)
	{
		const struct cpu_t *c1 = (const struct cpu_t *)a;
		const struct cpu_t *c2 = (const struct cpu_t *)b;
		if (c1->package_id == c2->package_id)
			if (c1->core_id == c2->core_id)
				return c1->id - c2->id;
			else
				return c1->core_id - c2->core_id;
		else
			return c1->package_id - c2->package_id;
	}
	qsort(system->cpus, system->cpu_count,
			sizeof(struct cpu_t), cmp);
}
