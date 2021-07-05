#include "map.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * Opens a file and returns its descriptor.
 */
int open(const char *path, int oflag, ...) {
    // convert flags field
    char mode[3] = {' ', ' ', 0};

    if(oflag & O_RDWR) {
        mode[0] = 'w';
        mode[1] = '+';
    }
    else if(oflag & O_RDONLY) {
        mode[0] = 'r';
    } else if(oflag & O_WRONLY) {
        mode[0] = 'w';
    }

    // open file
    FILE *f = fopen(path, mode);
    if(!f) {
        errno = ENOENT;
        return -1;
    }

    // return its descriptor name
    return fileno(f);
}
