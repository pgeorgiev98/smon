#ifndef SYSTEM_H_INCLUDED
#define SYSTEM_H_INCLUDED

struct cpu_t;

/** All the data about the system is stored here */
struct system_t
{
	int cpu_count; /**< The number of CPUs in the system */
	struct cpu_t *cpus; /**< All CPUs in the system ordered
						  by core_id and package_id */

	// File descriptors for files that are kept open
	int proc_stat_fd;

	// Generic buffer. Used when reading from /proc/stat
	char *buffer;
	int buffer_size;
};

struct system_t system_init();

void system_delete(struct system_t system);

/** Refresh all dynamically changing system stats */
void system_refresh_info(struct system_t *system);

#endif
