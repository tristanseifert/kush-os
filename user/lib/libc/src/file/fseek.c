#include <stdio.h>
#include <stdlib.h>
#include "file_private.h"

int fseek(FILE *stream, long offset, int whence) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

int fseeko(FILE *stream, off_t offset, int whence) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return -1;
}

int fsetpos(FILE *stream, const fpos_t *pos) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return -1;
}

long ftell(FILE *stream) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

off_t ftello(FILE *stream) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}
