#ifndef CPU_H_INCLUDED
#define CPU_H_INCLUDED

// CPU time
enum {
	CPU_USER_TIME = 0,
	CPU_NICE_TIME = 1,
	CPU_SYSTEM_TIME = 2,
	CPU_IDLE_TIME = 3,
	CPU_IOWAIT_TIME = 4,
	CPU_IRQ_TIME = 5,
	CPU_SOFTIRQ_TIME = 6,
	CPU_STEAL_TIME = 7,
	CPU_GUEST_TIME = 8,
	CPU_GUESTNICE_TIME = 9,
	CPU_STATS_COUNT = 10
};

/** A logical CPU */
struct cpu_t
{
	int id; /**< The ID of the CPU as it appears in /sys and /proc
			  e.g. cpu0, cpu3 */
	int core_id; /**< The ID of the CPU core */
	int package_id;  /**< The ID of the CPU package */

	int min_freq; /**< Minimum CPU frequency in KHz */
	int max_freq; /**< Maximum CPU frequency in KHz */
	int cur_freq; /**< Current CPU frequency in KHz */

	// CPU usage
	double total_usage; /**< The total usage for this cpu [0.0, 1.0] */
	int stats[CPU_STATS_COUNT]; /**< The last read time parameters from /proc/stat */

	int cur_temp; /**< The current core temperature in millidegree Celsius */

	// File descriptors for files that are kept open
	int cur_freq_fd;
	int cur_temp_fd;
};

#endif
