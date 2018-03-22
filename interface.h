#ifndef INTERFACE_H_INCLUDED
#define INTERFACE_H_INCLUDED

#define MAX_INTERFACE_NAME_LENGTH 15

/** A network interface */
struct interface_t
{
	char name[MAX_INTERFACE_NAME_LENGTH + 1]; /**< The interface name */

	unsigned long long delta_rx_bytes; /**< Bytes received since last checked */
	unsigned long long delta_tx_bytes; /**< Bytes transferred since last checked */

	unsigned long long last_total_rx_bytes; /**< Total bytes received */
	unsigned long long last_total_tx_bytes; /**< Total bytes transferred */

	// File descriptors for files that are kept open
	int rx_bytes_fd;
	int tx_bytes_fd;
};

#endif
