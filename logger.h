#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include <stdio.h>
#include "disk.h"
#include "interface.h"
#include "battery.h"

struct system_t;

enum logger_type
{
	CSV
};

struct logger_stat_t
{
	enum {
		LOGGER_CPU_FREQUENCY,
		LOGGER_CPU_USAGE,
		LOGGER_CPU_TEMPERATURE,
		LOGGER_RAM_USED,
		LOGGER_RAM_BUFFERS,
		LOGGER_RAM_CACHED,
		LOGGER_DISK_READ,
		LOGGER_DISK_WRITE,
		LOGGER_IFACE_READ,
		LOGGER_IFACE_WRITE,
		LOGGER_BAT_CHARGE,
		LOGGER_BAT_CURRENT,
		LOGGER_BAT_VOLTAGE
	} type;
	union logger_stat_data {
		int cpu_id;
		char iface_name[MAX_INTERFACE_NAME_LENGTH + 1];
		char disk_name[MAX_DISK_NAME_LENGTH + 1];
		char battery_name[MAX_BATTERY_NAME_LENGTH + 1];
	} data;
};

struct logger_t
{
	int type;
	FILE *file;
	int stat_count;
	struct logger_stat_t *stats;
};

int logger_init(struct logger_t *logger, int type,
		const char *filename, int stat_count, struct logger_stat_t *stats);

void logger_destroy(struct logger_t *logger);

void logger_log(struct logger_t *logger, struct system_t *system);

#endif
