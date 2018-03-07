#ifndef CPU_H_INCLUDED
#define CPU_H_INCLUDED

// CPU time
enum {
	USER_TIME = 0,
	NICE_TIME = 1,
	SYSTEM_TIME = 2,
	IDLE_TIME = 3,
	IOWAIT_TIME = 4,
	IRQ_TIME = 5,
	SOFTIRQ_TIME = 6,
	STEAL_TIME = 7,
	GUEST_TIME = 8,
	GUESTNICE_TIME = 9
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
	int time[10]; /**< The last read time parameters from /proc/stat */

	// File descriptors for files that are kept open
	int cur_freq_fd;
};

#endif
