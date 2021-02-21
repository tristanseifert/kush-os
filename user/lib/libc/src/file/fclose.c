#include <stdio.h>
#include <stdlib.h>
#include "file_private.h"

/**
 * Close the specified file descriptor.
 */
int fclose(FILE *stream) {
    // TODO: implement
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return -1;
}

/**
 * Closes all open descriptors.
 */
void fcloseall(void) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
}
