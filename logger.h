#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include <stdio.h>
#include "disk.h"
#include "interface.h"

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
		LOGGER_DISK_READ,
		LOGGER_DISK_WRITE,
		LOGGER_IFACE_READ,
		LOGGER_IFACE_WRITE
	} type;
	union logger_stat_data {
		int cpu_id;
		char iface_name[MAX_INTERFACE_NAME_LENGTH + 1];
		char disk_name[MAX_DISK_NAME_LENGTH + 1];
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
