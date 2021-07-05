#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/**
 * Writes the error message (for the current errno) to the console, as well as the (optional)
 * detail string, to standard error.
 */
void perror(const char *s) {
    if(s) {
        fprintf(stderr, "perror: errno = %s (%d): %s\n", strerror(errno), errno, s);
    } else {
        fprintf(stderr, "perror: errno = %s (%d)\n", strerror(errno), errno);
    }
}
