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
#include <dirent.h>
#include <sys/types.h>

#define CPU_DEVICES_DIR "/sys/bus/cpu/devices/"
static const int cpu_devices_dir_len = sizeof(CPU_DEVICES_DIR) - 1;
#define HWMON_DIR "/sys/class/hwmon/"
static const int hwmon_dir_len = sizeof(HWMON_DIR) - 1;
#define BLOCK_DEVICES_DIR "/sys/block/"
static const int block_devices_dir_len = sizeof(BLOCK_DEVICES_DIR) - 1;
#define INTERFACES_DIR "/sys/class/net/"
static const int interfaces_dir_len = sizeof(INTERFACES_DIR) - 1;
#define POWER_DIR "/sys/class/power_supply/"
static const int power_dir_len = sizeof(POWER_DIR) - 1;
#define PROC_STAT_DIR "/proc/stat"
#define MEMINFO_PATH "/proc/meminfo"


static void system_cpu_init(struct system_t *);
static void system_disk_init(struct system_t *);
static void system_net_init(struct system_t *);
static void system_bat_init(struct system_t *);

struct system_t system_init(void)
{
	struct system_t system;

	system.proc_stat_fd = open_file_readonly(PROC_STAT_DIR);
	system.meminfo_fd = open_file_readonly(MEMINFO_PATH);

	system_cpu_init(&system);
	system_disk_init(&system);
	system_net_init(&system);
	system_bat_init(&system);

	system.buffer = NULL;
	system.buffer_size = 0;

	system_refresh_info(&system);

	return system;
}

void system_delete(struct system_t system)
{
	// Close files
	close(system.proc_stat_fd);
	close(system.meminfo_fd);
	for (int i = 0; i < system.cpu_count; ++i)
		close(system.cpus[i].cur_freq_fd);
	for (int i = 0; i < system.disk_count; ++i)
		close(system.disks[i].stat_fd);
	for (int i = 0; i < system.interface_count; ++i) {
		close(system.interfaces[i].rx_bytes_fd);
		close(system.interfaces[i].tx_bytes_fd);
	}
	for (int i = 0; i < system.battery_count; ++i) {
		close(system.batteries[i].charge_fd);
		close(system.batteries[i].current_fd);
		close(system.batteries[i].voltage_fd);
	}

	// Free memory
	free(system.buffer);
	free(system.cpus);
	free(system.disks);
	free(system.interfaces);
	free(system.batteries);
}


static void system_refresh_cpus(struct system_t *system);
static void system_refresh_ram(struct system_t *system);
static void system_refresh_disks(struct system_t *system);
static void system_refresh_interfaces(struct system_t *system);
static void system_refresh_batteries(struct system_t *system);

void system_refresh_info(struct system_t *system)
{
	system_refresh_cpus(system);
	system_refresh_ram(system);
	system_refresh_disks(system);
	system_refresh_interfaces(system);
	system_refresh_batteries(system);
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
	if (cpu_devices_dir == NULL)
		return;
	struct dirent *cpu_ent;
	while ((cpu_ent = readdir(cpu_devices_dir))) {
		// Skip entities not starting with "cpu"
		if (strncmp(cpu_ent->d_name, "cpu", 3))
			continue;

		struct cpu_t cpu;
		for (int i = 0; i < CPU_STATS_COUNT; ++i)
			cpu.stats[i] = 0;
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

		// Set the current cpu temperature to the default
		cpu.cur_temp = 0;

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

static void system_bat_init(struct system_t *system)
{
	system->batteries = NULL;
	system->battery_count = 0;
	system->max_battery_count = 0;
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

	// Get the cpu core temperatures

	// Set all cpu temps to the default
	for (int i = 0; i < system->cpu_count; ++i)
		system->cpus[i].cur_temp = 0;

	// Open /sys/class/hwmon/
	DIR *hwmon_dir = opendir(HWMON_DIR);
	if (hwmon_dir == NULL)
		return;
	char filename[hwmon_dir_len + 100];
	strcpy(filename, HWMON_DIR);
	// List /sys/class/hwmon/
	struct dirent *hwmon_ent;
	while ((hwmon_ent = readdir(hwmon_dir))) {
		const char *hwmon_subdir_name = hwmon_ent->d_name;
		const int hwmon_subdir_name_len = strlen(hwmon_subdir_name);
		// Ignore dotfiles
		if (hwmon_subdir_name[0] == '.')
			continue;

		// Open /sys/class/hwmon/$hwmon_subdir_name
		strcpy(filename + hwmon_dir_len, hwmon_subdir_name);
		strcpy(filename + hwmon_dir_len + hwmon_subdir_name_len, "/");
		DIR *hwmon_subdir = opendir(filename);
		if (hwmon_subdir == NULL)
			continue;
		// List /sys/class/hwmon/$hwmon_subdir_name
		struct dirent *hwmon_subent;
		while ((hwmon_subent = readdir(hwmon_subdir))) {
			// We're looking for files that match /^temp[0-9]+_label$/
			const char *fnm = hwmon_subent->d_name;
			// Name starts with temp
			if (strncmp(fnm, "temp", 4))
				continue;
			// Followed by at least one digit
			int digits = 0;
			for (; fnm[4 + digits] >= '0' && fnm[4 + digits] <= '9'; ++digits);
			if (digits == 0)
				continue;
			// Followed by "_label"
			if (strncmp(fnm + 4 + digits, "_label", 6))
				continue;
			// And that's it
			if (fnm[4 + digits + 6] != '\0')
				continue;

			// We found our file, now open it and read it's contents
			strcpy(filename + hwmon_dir_len + hwmon_subdir_name_len + 1, fnm);
			char contents[32];
			contents[read_file_to_string(filename, contents, sizeof(contents) - 1)] = '\0';
			// We hope we just read something like /^Core [0-9]+$/
			// Contents start with "Core "
			if (strncmp(contents, "Core ", 5))
				continue;
			// Followed by at least one digit (that's out core id)
			int core_id_len = 0;
			int core_id = 0;
			for (; contents[5 + core_id_len] >= '0' && contents[5 + core_id_len] <= '9'; ++core_id_len)
				core_id = core_id * 10 + contents[5 + core_id_len] - '0';
			if (core_id_len == 0)
				continue;
			// And that's it
			if (contents[5 + core_id_len] != '\n')
				continue;

			// Now let's try to read the file that has the actual core temperature
			char temp_filename[4 + digits + 6 + 1];;
			strncpy(temp_filename, fnm, 4 + digits);
			strcpy(temp_filename + 4 + digits, "_input");
			strcpy(filename + hwmon_dir_len + hwmon_subdir_name_len + 1, temp_filename);
			contents[read_file_to_string(filename, contents, sizeof(contents) - 1)] = '\0';
			if (contents[0] == '\0')
				continue;
			int temperature = atoi(contents);

			// Set it to all CPUs with this core id
			for (int i = 0; i < system->cpu_count; ++i)
				if (system->cpus[i].core_id == core_id)
					system->cpus[i].cur_temp = temperature;
		}
		closedir(hwmon_subdir);
	}
	closedir(hwmon_dir);
}

static void system_refresh_ram(struct system_t *system)
{
	// Read /proc/meminfo
	lseek(system->meminfo_fd, 0, SEEK_SET);
	const int MAX_MEMINFO_LEN = 4096;
	char meminfo[MAX_MEMINFO_LEN];
	int bytes = read_fd_to_string(system->meminfo_fd, meminfo,
			MAX_MEMINFO_LEN);
	long long mem_total = -1, mem_free = -1,
		 mem_buffers = -1, mem_cached = -1,
		 mem_reclaimable = -1, mem_shared = -1;

	char key[128];
	for (int i = 0; i < bytes;) {
		// Read the key
		int key_len = 0;
		for (; i < bytes && meminfo[i] != ':' && key_len < 127; ++i)
			key[key_len++] = meminfo[i];
		if (i == bytes)
			break;
		key[key_len] = '\0';

		// Find the begining of the value
		for (; i < bytes && (meminfo[i] < '0' || meminfo[i] > '9'); ++i);

		// Read the value
		int value = 0;
		for (; i < bytes && meminfo[i] >= '0' && meminfo[i] <= '9'; ++i)
			value = value * 10 + (meminfo[i] - '0');

		// Find the end of the line
		for (; i < bytes && meminfo[i - 1] != '\n'; ++i);

		// If we have successfully read the whole line...
		if (meminfo[i - 1] == '\n') {
			if (!strcmp(key, "MemTotal"))
				mem_total = value;
			else if (!strcmp(key, "MemFree"))
				mem_free = value;
			else if (!strcmp(key, "Buffers"))
				mem_buffers = value;
			else if (!strcmp(key, "Cached"))
				mem_cached = value;
			else if (!strcmp(key, "SReclaimable"))
				mem_reclaimable = value;
			else if (!strcmp(key, "Shmem"))
				mem_shared = value;
		}
	}

	int total_used = mem_total - mem_free;
	int cached = mem_cached + mem_reclaimable - mem_shared;
	int used = total_used - mem_buffers - cached;
	system->ram_buffers = mem_buffers * 1024LL;
	system->ram_cached = cached * 1024LL;
	system->ram_used = used * 1024LL;
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
			for (int i = 0; i < DISK_STATS_COUNT; ++i)
				disk.last_stats[i] = 0;

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
		stat_buffer[bytes_read] = '\0';

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
			interface.last_total_rx_bytes = 0;
			interface.last_total_tx_bytes = 0;

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

static void system_refresh_batteries(struct system_t *system)
{
	// Will be used to store the path to various files
	char filepath[power_dir_len + MAX_BATTERY_NAME_LENGTH + 32];
	strcpy(filepath, POWER_DIR);

	// Open /sys/class/power_supply/
	DIR *power_dir = opendir(POWER_DIR);
	if (power_dir == NULL) {
		system->battery_count = 0;
		return;
	}
	// List /sys/class/power_supply/
	struct dirent *power_ent;
	while ((power_ent = readdir(power_dir))) {
		// Ignore devices with too long names and
		// directories with names not starting with BAT
		if (strlen(power_ent->d_name) > MAX_BATTERY_NAME_LENGTH ||
				strncmp(power_ent->d_name, "BAT", 3))
			continue;


		// Check for an known battery with the same name
		struct battery_t *batptr = NULL;
		for (int i = 0; i < system->battery_count; ++i) {
			if (strcmp(power_ent->d_name,
						system->batteries[i].name) == 0) {
				batptr = &system->batteries[i];
				break;
			}
		}
		// If it's not found, add it
		if (batptr == NULL) {
			struct battery_t battery;

			// Set the battery name
			strcpy(battery.name, power_ent->d_name);

			// Append the name to the path
			strcpy(filepath + power_dir_len, battery.name);

			// Open the charge file
			strcpy(filepath + power_dir_len +
					strlen(battery.name), "/capacity");
			battery.charge_fd = open_file_readonly(filepath);
			if (battery.charge_fd == -1)
				continue;

			// Open the current_now file
			strcpy(filepath + power_dir_len +
					strlen(battery.name), "/current_now");
			battery.current_fd = open_file_readonly(filepath);
			if (battery.current_fd == -1) {
				close(battery.charge_fd);
				continue;
			}

			// Open the voltage_now file
			strcpy(filepath + power_dir_len +
					strlen(battery.name), "/voltage_now");
			battery.voltage_fd = open_file_readonly(filepath);
			if (battery.voltage_fd == -1) {
				close(battery.charge_fd);
				close(battery.current_fd);
				continue;
			}


			// Allocate memory if necessary
			if (system->battery_count == system->max_battery_count) {
				system->max_battery_count += 128;
				system->batteries = (struct battery_t *)realloc(
						system->batteries,
						sizeof(struct battery_t) *
						system->max_battery_count);
			}
			system->batteries[system->battery_count++] = battery;
		}
	}
	closedir(power_dir);

	// Loop over all batteries
	for (int i = 0; i < system->battery_count; ++i) {
		struct battery_t *battery = &system->batteries[i];

		// Read the battery stats

		lseek(battery->charge_fd, 0, SEEK_SET);
		battery->charge = read_int_from_fd(battery->charge_fd);

		lseek(battery->current_fd, 0, SEEK_SET);
		battery->current = read_int_from_fd(battery->current_fd);

		lseek(battery->voltage_fd, 0, SEEK_SET);
		battery->voltage = read_int_from_fd(battery->voltage_fd);
	}
}
