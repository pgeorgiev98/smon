#include "logger.h"
#include "system.h"
#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int logger_init(struct logger_t *logger, int type,
		const char *filename, int stat_count, struct logger_stat_t *stats)
{
	logger->type = type;
	logger->stat_count = stat_count;
	logger->file = NULL;
	logger->stats = NULL;
	if (stat_count == 0)
		return 0;

	logger->stats = (struct logger_stat_t *)malloc(
			sizeof(struct logger_stat_t) * stat_count);
	if (logger->stats == NULL)
		return 1;
	for (int i = 0; i < stat_count; ++i)
		logger->stats[i] = stats[i];

	logger->file = fopen(filename, "w");
	if (logger->file == NULL) {
		free(logger->stats);
		return 2;
	}

	for (int i = 0; i < logger->stat_count; ++i) {
		struct logger_stat_t stat = logger->stats[i];
		char value[128];
		if (stat.type == LOGGER_CPU_USAGE)
			sprintf(value, "CPU%d Usage", stat.data.cpu_id);
		else if (stat.type == LOGGER_CPU_TEMPERATURE)
			sprintf(value, "CPU%d Temperature", stat.data.cpu_id);
		else if (stat.type == LOGGER_CPU_FREQUENCY)
			sprintf(value, "CPU%d Frequency (KHz)", stat.data.cpu_id);
		else if (stat.type == LOGGER_DISK_READ)
			sprintf(value, "Disk %s Read Speed (B/s)", stat.data.disk_name);
		else if (stat.type == LOGGER_DISK_WRITE)
			sprintf(value, "Disk %s Write Speed (B/s)", stat.data.disk_name);
		else if (stat.type == LOGGER_IFACE_READ)
			sprintf(value, "Interface %s Download Speed (B/s)", stat.data.iface_name);
		else if (stat.type == LOGGER_IFACE_WRITE)
			sprintf(value, "Interface %s Upload Speed (B/s)", stat.data.iface_name);
		else
			value[0] = '\0';

		fprintf(logger->file, "%s", value);
		fputc(i == logger->stat_count - 1 ? '\n' : ',', logger->file);
	}
	return 0;
}

void logger_destroy(struct logger_t *logger)
{
	free(logger->stats);
	if (logger->file)
		fclose(logger->file);
}

void logger_log(struct logger_t *logger, struct system_t *system)
{
	for (int i = 0; i < logger->stat_count; ++i) {
		struct logger_stat_t stat = logger->stats[i];
		char value[128];
		if (stat.type == LOGGER_CPU_FREQUENCY) {
			sprintf(value, "%d", system->cpus[stat.data.cpu_id].cur_freq);
		} else if (stat.type == LOGGER_CPU_USAGE) {
			sprintf(value, "%f", system->cpus[stat.data.cpu_id].total_usage * 100.0);
		} else if (stat.type == LOGGER_CPU_TEMPERATURE) {
			sprintf(value, "%f", system->cpus[stat.data.cpu_id].cur_temp / 1000.0);
		} else if (stat.type == LOGGER_DISK_READ || stat.type == LOGGER_DISK_WRITE) {
			struct disk_t *disk = NULL;
			for (int i = 0; i < system->disk_count; ++i) {
				if (!strcmp(system->disks[i].name, stat.data.disk_name)) {
					disk = &system->disks[i];
					break;
				}
			}
			int disk_stat = stat.type == LOGGER_DISK_READ ? DISK_READ_SECTORS : DISK_WRITE_SECTORS;
			sprintf(value, "%d", disk ? disk->stats_delta[disk_stat] * 512 : 0);
		} else if (stat.type == LOGGER_IFACE_READ || stat.type == LOGGER_IFACE_WRITE) {
			struct interface_t *interface = NULL;
			for (int i = 0; i < system->interface_count; ++i) {
				if (!strcmp(system->interfaces[i].name, stat.data.iface_name)) {
					interface = &system->interfaces[i];
					break;
				}
			}
			sprintf(value, "%llu", interface ? ( stat.type == LOGGER_IFACE_READ ?
						interface->delta_rx_bytes : interface->delta_tx_bytes) : 0);
		} else {
			value[0] = '\0';
		}

		fprintf(logger->file, "%s", value);
		fputc(i == logger->stat_count - 1 ? '\n' : ',', logger->file);
	}
}
