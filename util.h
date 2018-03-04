#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

/** Writes the contents of file 'filename' to out */
int read_file_to_string(const char *filename, char *out, int maxbytes);

/** Read an int value from a file */
int read_int_from_file(const char *filename);

#endif
