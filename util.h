#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

/** Open a file for reading */
int open_file_readonly(const char *filename);


/** Writes the contents of opened file to out */
int read_fd_to_string(int fd, char *out, int maxbytes);

/** Writes the contents of file 'filename' to out */
int read_file_to_string(const char *filename, char *out, int maxbytes);


/** Read an int value from an already opened file */
int read_int_from_fd(int fd);

/** Read an unsigned long long int from an already opened file */
unsigned long long read_ull_from_fd(int fd);

/** Read an int value from a file */
int read_int_from_file(const char *filename);


/** Converts bytes to a human readable string (e.g. 37 MiB) */
void bytes_to_human_readable(unsigned long long bytes, char *out);

#endif
