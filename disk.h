#ifndef DISK_H_INCLUDED
#define DISK_H_INCLUDED

// Disk stats
enum
{
	DISK_READ_IO = 0,
	DISK_READ_MERGES = 1,
	DISK_READ_SECTORS = 2,
	DISK_READ_TICKS = 3,
	DISK_WRITE_IO = 4,
	DISK_WRITE_MERGES = 5,
	DISK_WRITE_SECTORS = 6,
	DISK_WRITE_TICKS = 7,
	DISK_STATS_COUNT = 8
};

#define MAX_DISK_NAME_LENGTH 31

/** A block device */
struct disk_t
{
	char name[MAX_DISK_NAME_LENGTH + 1]; /**< The disk name */

	int stats_delta[DISK_STATS_COUNT]; /**< The change of the stats */
	int last_stats[DISK_STATS_COUNT]; /**< The values from the stat file */

	// File descriptors for files that are kept open
	int stat_fd;
};

#endif
