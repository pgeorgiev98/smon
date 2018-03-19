#ifndef DISK_H_INCLUDED
#define DISK_H_INCLUDED

// Disk stats
enum
{
	READ_IO = 0,
	READ_MERGES = 1,
	READ_SECTORS = 2,
	READ_TICKS = 3,
	WRITE_IO = 4,
	WRITE_MERGES = 5,
	WRITE_SECTORS = 6,
	WRITE_TICKS = 7,
	STATS_COUNT = 8
};

#define MAX_DISK_NAME_LENGTH 7

struct disk_t
{
	char name[MAX_DISK_NAME_LENGTH + 1];
	int stat_fd;

	int stats_delta[STATS_COUNT];
	int last_stats[STATS_COUNT];
};

#endif
