#include <errno.h>
#include <stdio.h>
#include <time.h>

/**
 * Returns the number of seconds since the UNIX epoch (midnight at Jan 1, 1970) WITHOUT any
 * considerations for locale-specific stuff (leap seconds)
 *
 * @return Seconds value, or -1 on error.
 */
time_t time(time_t *outTime) {
    time_t time = 0;

    // TODO: implement
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    if(outTime) *outTime = time;
    return time;
}