#ifndef BATTERY_H_INCLUDED
#define BATTERY_H_INCLUDED

#define MAX_BATTERY_NAME_LENGTH 7

/** A battery */
struct battery_t
{
	char name[MAX_BATTERY_NAME_LENGTH + 1]; /**< The name of the battery as it
											  in /sys/class/power_supply/ */

	int charge; /**< The battery charge percentage */
	int current; /**< The battery current in uA */
	int voltage; /**< The battery voltage in uV */

	// File descriptors for files that are kept open
	int charge_fd;
	int current_fd;
	int voltage_fd;
};

#endif
