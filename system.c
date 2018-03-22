#include "system.h"
#include "util.h"
#include "cpu.h"
#include "disk.h"
#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#define CPU_DEVICES_DIR "/sys/bus/cpu/devices/"
static const int cpu_devices_dir_len = sizeof(CPU_DEVICES_DIR) - 1;
#define BLOCK_DEVICES_DIR "/sys/block/"
static const int block_devices_dir_len = sizeof(BLOCK_DEVICES_DIR) - 1;
#define INTERFACES_DIR "/sys/class/net/"
static const int interfaces_dir_len = sizeof(INTERFACES_DIR) - 1;
#define PROC_STAT_DIR "/proc/stat"


static void system_cpu_init(struct system_t *);
static void system_disk_init(struct system_t *);
static void system_net_init(struct system_t *);

struct system_t system_init()
{
	struct system_t system;

	system.proc_stat_fd = open_file_readonly(PROC_STAT_DIR);

	system_cpu_init(&system);
	system_disk_init(&system);
	system_net_init(&system);

	system.buffer = NULL;
	system.buffer_size = 0;

	system_refresh_info(&system);

	return system;
}

void system_delete(struct system_t system)
{
	// Close files
	close(system.proc_stat_fd);
	for (int i = 0; i < system.cpu_count; ++i)
		close(system.cpus[i].cur_freq_fd);
	for (int i = 0; i < system.disk_count; ++i)
		close(system.disks[i].stat_fd);
	for (int i = 0; i < system.interface_count; ++i) {
		close(system.interfaces[i].rx_bytes_fd);
		close(system.interfaces[i].tx_bytes_fd);
	}

	// Free memory
	free(system.cpus);
	free(system.disks);
	free(system.interfaces);
}


static void system_refresh_cpus(struct system_t *system);
static void system_refresh_disks(struct system_t *system);
static void system_refresh_interfaces(struct system_t *system);

void system_refresh_info(struct system_t *system)
{
	system_refresh_cpus(system);
	system_refresh_disks(system);
	system_refresh_interfaces(system);
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

static void system_disk_init(struct system_t *system)
{
	system->disks = NULL;
	system->disk_count = 0;
	system->max_disk_count = 0;
}

static void system_net_init(struct system_t *system)
{
	system->interfaces = NULL;
	system->interface_count = 0;
	system->max_interface_count = 0;
}


// Refresh system CPU stats
static void system_refresh_cpus(struct system_t *system)
{
	// Update current CPU frequency
	for (int i = 0; i < system->cpu_count; ++i) {
		struct cpu_t *cpu = &system->cpus[i];
		lseek(cpu->cur_freq_fd, 0, SEEK_SET);
		cpu->cur_freq = read_int_from_fd(cpu->cur_freq_fd);
	}

	// Read the whole /proc/stat file
	lseek(system->proc_stat_fd, 0, SEEK_SET);
	int len = 0;
	for (;;) {
		if (system->buffer_size == len) {
			system->buffer_size += 2048;
			system->buffer = (char *)realloc(system->buffer,
					system->buffer_size);
		}

		int bytes_to_read = system->buffer_size - len;

		int bytes_read = read_fd_to_string(system->proc_stat_fd,
				system->buffer + len, bytes_to_read);
		if (bytes_read <= 0)
			break;
		len += bytes_read;
	}
	system->buffer[len] = '\0';

	// Skip the first line
	int i = 0;
	while (i < len && system->buffer[i] != '\n')
		++i;

	// For each cpuN line in /proc/stat
	for (;;) {
		// Make sure this line starts with cpuN
		char cpuname[16];
		int bytes;
		int fields_read = sscanf(system->buffer + i,
				"%s%n", cpuname, &bytes);
		i += bytes;
		if (fields_read <= 0 || strncmp(cpuname, "cpu", 3) != 0)
			break;

		// Get the cpu id (the N in cpuN)
		int cpu_id = atoi(cpuname + 3);
		struct cpu_t *cpu = NULL;
		for (int c = 0; c < system->cpu_count; ++c) {
			if (system->cpus[c].id == cpu_id) {
				cpu = &system->cpus[c];
				break;
			}
		}

		// Calculate the cpu usage

		int stats[CPU_STATS_COUNT];
		for (int t = 0; t < CPU_STATS_COUNT; ++t) {
			sscanf(system->buffer + i, "%d%n", &stats[t], &bytes);
			i += bytes;
		}

		int delta_stats[CPU_STATS_COUNT];
		for (int t = 0; t < CPU_STATS_COUNT; ++t)
			delta_stats[t] = stats[t] - cpu->stats[t];


		int total_cpu_time = 0;
		for (int t = CPU_USER_TIME; t <= CPU_STEAL_TIME; ++t)
			total_cpu_time += delta_stats[t];

		int idle_cpu_time = delta_stats[CPU_IDLE_TIME] +
							delta_stats[CPU_IOWAIT_TIME];

		cpu->total_usage = (double)(total_cpu_time - idle_cpu_time) /
							total_cpu_time;

		// Make sure the value is in [0.0, 1.0]
		// It will also change nan values to 0.0
		if (!(cpu->total_usage >= 0.0))
			cpu->total_usage = 0.0;
		else if (cpu->total_usage > 1.0)
			cpu->total_usage = 1.0;

		for (int f = 0; f < CPU_STATS_COUNT; ++f)
			cpu->stats[f] = stats[f];
	}
}

static void system_refresh_disks(struct system_t *system)
{
	// Will be used to store the path to the stat file for each device
	char filepath[block_devices_dir_len + 20];
	strcpy(filepath, BLOCK_DEVICES_DIR);

	// Open /sys/block/
	DIR *block_devices_dir = opendir(BLOCK_DEVICES_DIR);
	if (block_devices_dir == NULL) {
		system->disk_count = 0;
		return;
	}
	// List /sys/block
	struct dirent *block_device_ent;
	while ((block_device_ent = readdir(block_devices_dir))) {
		// Ignore loop devices, dotfiles and devices with
		// too long names
		if (strncmp(block_device_ent->d_name, "loop", 4) == 0 ||
				block_device_ent->d_name[0] == '.' ||
				strlen(block_device_ent->d_name) >
					MAX_DISK_NAME_LENGTH)
			continue;

		// Check for a disk with the same name in system
		struct disk_t *diskptr = NULL;
		for (int i = 0; i < system->disk_count; ++i) {
			if (strcmp(block_device_ent->d_name,
						system->disks[i].name) == 0) {
				diskptr = &system->disks[i];
				break;
			}
		}
		// If it's not found, add it
		if (diskptr == NULL) {
			struct disk_t disk;

			// Set the disk name
			strcpy(disk.name, block_device_ent->d_name);

			// Open the stat file
			strcpy(filepath + block_devices_dir_len, disk.name);
			strcpy(filepath + block_devices_dir_len +
					strlen(disk.name), "/stat");
			disk.stat_fd = open_file_readonly(filepath);
			if (disk.stat_fd == -1) {
				continue;
			}

			// Allocate memory if necessary
			if (system->disk_count == system->max_disk_count) {
				system->max_disk_count += 128;
				system->disks = (struct disk_t *)realloc(
						system->disks,
						sizeof(struct disk_t) * system->max_disk_count);
			}
			system->disks[system->disk_count++] = disk;
		}
	}
	closedir(block_devices_dir);

	// Loop over all disks
	for (int d = 0; d < system->disk_count; ++d) {
		struct disk_t *disk = &system->disks[d];

		// Read the disk stats
		lseek(disk->stat_fd, 0, SEEK_SET);
		const int stat_buffer_size = 511; // Should be enough
		char stat_buffer[stat_buffer_size + 1];
		int bytes_read = read_fd_to_string(disk->stat_fd,
				stat_buffer, stat_buffer_size);
		if (bytes_read <= 0) {
			// On error, try to reopen the stat file
			close(disk->stat_fd);
			char filename[block_devices_dir_len + 20];
			strcpy(filename, BLOCK_DEVICES_DIR);
			strcpy(filename + block_devices_dir_len, disk->name);
			strcpy(filename + block_devices_dir_len +
					strlen(disk->name), "/stat");
			if ((disk->stat_fd = open_file_readonly(filename)) < 0)
				continue;
			bytes_read = read_fd_to_string(disk->stat_fd,
					stat_buffer, stat_buffer_size);
		}
		if (bytes_read <= 0) {
			close(disk->stat_fd);
			// Mark it with -1 so that we can delete it
			// from the array
			disk->stat_fd = -1;
			continue;
		}
		stat_buffer[stat_buffer_size] = '\0';

		// Save the stats
		int buf_index = 0;
		for (int s = 0; s < DISK_STATS_COUNT; ++s) {
			int bytes_read;
			int stat;
			sscanf(stat_buffer + buf_index, "%d%n",
					&stat, &bytes_read);
			disk->stats_delta[s] = stat - disk->last_stats[s];
			disk->last_stats[s] = stat;
			buf_index += bytes_read;
		}
	}

	// Remove unneeded disks from array
	int i = 0;
	for (int j = 0; j < system->disk_count; ++j) {
		if (system->disks[j].stat_fd >= 0) {
			if (i != j)
				system->disks[i] = system->disks[j];
			++i;
		}
	}
	system->disk_count = i;
}

static void system_refresh_interfaces(struct system_t *system)
{
	// Will be used to store the path to various files
	char filepath[interfaces_dir_len + MAX_INTERFACE_NAME_LENGTH + 32];
	strcpy(filepath, INTERFACES_DIR);

	// Open /sys/class/net/
	DIR *interfaces_dir = opendir(INTERFACES_DIR);
	if (interfaces_dir == NULL) {
		system->interface_count = 0;
		return;
	}
	// List /sys/class/net/
	struct dirent *interface_ent;
	while ((interface_ent = readdir(interfaces_dir))) {
		// Ignore dotfiles and devices with too long names
		if (interface_ent->d_name[0] == '.' ||
				strlen(interface_ent->d_name) >
				MAX_INTERFACE_NAME_LENGTH)
			continue;


		// Check for an known interface with the same name
		struct interface_t *ifaceptr = NULL;
		for (int i = 0; i < system->interface_count; ++i) {
			if (strcmp(interface_ent->d_name,
						system->interfaces[i].name) == 0) {
				ifaceptr = &system->interfaces[i];
				break;
			}
		}
		// If it's not found, add it
		if (ifaceptr == NULL) {
			struct interface_t interface;

			// Set the interface name
			strcpy(interface.name, interface_ent->d_name);

			// Append the name to the path
			strcpy(filepath + interfaces_dir_len, interface.name);

			// Open the rx_bytes file
			strcpy(filepath + interfaces_dir_len +
					strlen(interface.name), "/statistics/rx_bytes");
			interface.rx_bytes_fd = open_file_readonly(filepath);
			if (interface.rx_bytes_fd == -1) {
				continue;
			}

			// Open the tx_bytes file
			strcpy(filepath + interfaces_dir_len +
					strlen(interface.name), "/statistics/tx_bytes");
			interface.tx_bytes_fd = open_file_readonly(filepath);
			if (interface.tx_bytes_fd == -1) {
				close(interface.rx_bytes_fd);
				continue;
			}

			// Allocate memory if necessary
			if (system->interface_count == system->max_interface_count) {
				system->max_interface_count += 128;
				system->interfaces = (struct interface_t *)realloc(
						system->interfaces,
						sizeof(struct interface_t) *
						system->max_interface_count);
			}
			system->interfaces[system->interface_count++] = interface;
		}
	}
	closedir(interfaces_dir);

	// Loop over all interfaces
	for (int i = 0; i < system->interface_count; ++i) {
		struct interface_t *interface = &system->interfaces[i];

		// Read the interface stats

		lseek(interface->rx_bytes_fd, 0, SEEK_SET);
		unsigned long long rx = read_ull_from_fd(interface->rx_bytes_fd);
		interface->delta_rx_bytes = rx - interface->last_total_rx_bytes;
		interface->last_total_rx_bytes = rx;

		lseek(interface->tx_bytes_fd, 0, SEEK_SET);
		unsigned long long tx = read_ull_from_fd(interface->tx_bytes_fd);
		interface->delta_tx_bytes = tx - interface->last_total_tx_bytes;
		interface->last_total_tx_bytes = tx;

	}
}
