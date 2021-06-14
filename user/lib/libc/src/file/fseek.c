#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "file_private.h"

int fseek(FILE *stream, long offset, int whence) {
    if(stream->seek) {
        return stream->seek(stream, offset, whence);
    }

    errno = ENODEV;
    return -1;
}

// XXX: this was simply copied from fseek; is this going to cause any problems?
int fseeko(FILE *stream, off_t offset, int whence) {
    if(stream->seek) {
        return stream->seek(stream, offset, whence);
    }

    errno = ENODEV;
    return -1;
}

int fsetpos(FILE *stream, const fpos_t *pos) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return -1;
}

long ftell(FILE *stream) {
    if(stream->tell) {
        long ret = 0;
        int err = stream->tell(stream, &ret);

        if(err >= 0) {
            return ret;
        } else {
            return err;
        }
    }

    errno = ENODEV;
    return -1;
}

off_t ftello(FILE *stream) {
    if(stream->tell) {
        long ret = 0;
        int err = stream->tell(stream, &ret);

        if(err >= 0) {
            return ret;
        } else {
            return err;
        }
    }

    errno = ENODEV;
    return -1;
}
