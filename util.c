#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int open_file_readonly(const char *filename)
{
	return open(filename, O_RDONLY);
}


int read_fd_to_string(int fd, char *out, int maxbytes)
{
	int bytes_read = 0;
	int bytes;
	while ((bytes = read(fd, out + bytes_read,
						 maxbytes - bytes_read)) > 0)
		bytes_read += bytes;
	return bytes_read;
}

int read_file_to_string(const char *filename, char *out, int maxbytes)
{
	int fd = open_file_readonly(filename);
	return read_fd_to_string(fd, out, maxbytes);
}


int read_int_from_fd(int fd)
{
	char buf[16];
	int l = read_fd_to_string(fd, buf, 15);
	buf[l] = '\0';
	return atoi(buf);
}

unsigned long long read_ull_from_fd(int fd)
{
	char buf[24];
	int l = read_fd_to_string(fd, buf, 23);
	buf[l] = '\0';
	return strtoull(buf, NULL, 10);
}

int read_int_from_file(const char *filename)
{
	char buf[16];
	int l = read_file_to_string(filename, buf, 15);
	buf[l] = '\0';
	return atoi(buf);
}


void bytes_to_human_readable(unsigned long long bytes, char *out)
{
	static const char * const units[] = {
		"B", "KiB", "MiB", "GiB", "TiB",
		"PiB", "EiB", "ZiB", "YiB"
	};
	static int units_count = sizeof(units) / sizeof(char *);

	int u, d = 0;
	for (u = 0; u < units_count && bytes >= 1024; ++u) {
		d = ((bytes % 1024) / 1024.0f) * 10;
		bytes /= 1024;
	}
	sprintf(out, "%llu.%d%s", bytes, d, units[u]);
}

