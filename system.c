#include "system.h"
#include "cpu.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
	for (int i = 0; i < system.cpu_count; ++i) {
		close(system.cpus[i].cur_freq_fd);
	}
	free(system.cpus);
}


static void system_refresh_cpu_info(struct system_t *system);

void system_refresh_info(struct system_t *system)
{
	system_refresh_cpu_info(system);
}


// Comparison function for sorting CPUs
static int cpu_cmp(const void *a, const void *b)
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
		
		// Load static CPU parameters
		strcpy(fname + cpu_dir_len, "/topology/core_id");
		cpu.core_id = read_int_from_file(fname);

		strcpy(fname + cpu_dir_len, "/topology/physical_package_id");
		cpu.package_id = read_int_from_file(fname);

		strcpy(fname + cpu_dir_len, "/cpufreq/cpuinfo_max_freq");
		cpu.max_freq = read_int_from_file(fname);

		strcpy(fname + cpu_dir_len, "/cpufreq/cpuinfo_min_freq");
		cpu.min_freq = read_int_from_file(fname);

		// Open the files for the dynamic CPU parameters
		strcpy(fname + cpu_dir_len, "/cpufreq/scaling_cur_freq");
		cpu.cur_freq_fd = open_file_readonly(fname);

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
	qsort(system->cpus, system->cpu_count,
			sizeof(struct cpu_t), cpu_cmp);
}


// Refresh system CPU stats
static void system_refresh_cpu_info(struct system_t *system)
{
	for (int i = 0; i < system->cpu_count; ++i) {
		struct cpu_t *cpu = &system->cpus[i];
		lseek(cpu->cur_freq_fd, 0, SEEK_SET);
		cpu->cur_freq = read_int_from_fd(cpu->cur_freq_fd);
	}
}
