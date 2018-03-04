#ifndef CPU_H_INCLUDED
#define CPU_H_INCLUDED

/** A logical CPU */
struct cpu_t
{
	int id; /**< The ID of the CPU as it appears in /sys and /proc
			  e.g. cpu0, cpu3 */
	int core_id; /**< The ID of the CPU core */
	int package_id;  /**< The ID of the CPU package */
};

#endif
