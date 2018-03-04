#include "util.h"

#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int read_file_to_string(const char *filename, char *out, int maxbytes)
{
	int fd = open(filename, O_RDONLY);
	int bytes_read = 0;
	int bytes;
	while ((bytes = read(fd, out + bytes_read, maxbytes - bytes_read)))
		bytes_read += bytes;
	close(fd);
	return bytes_read;
}

int read_int_from_file(const char *filename)
{
	char buf[16];
	int l = read_file_to_string(filename, buf, 15);
	buf[l] = '\0';
	return atoi(buf);
}
