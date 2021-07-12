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

/**
 * Converts the provided timestamp to a time struct (in UTC) in the provided buffer.
 */
struct tm *gmtime_r(const time_t *timep, struct tm *result) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return NULL;
}

/**
 * Converts the provided timestamp to a time struct, in UTC.
 */
struct tm *gmtime(const time_t *timep) {
    static struct tm tm;
    return gmtime_r(timep, &tm);
}

/**
 * Converts the provided timestamp to a time struct (in local time) in the provided buffer.
 */
struct tm *localtime_r(const time_t *clock, struct tm *result) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return NULL;
}

/**
 * Converts the provided timestamp to a time struct, in local time.
 */
struct tm *localtime(const time_t *timep) {
    static struct tm tm;
    return localtime_r(timep, &tm);
}

/**
 * Returns the difference between two times.
 */
double difftime(time_t time1, time_t time0) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

/**
 * Converts the given time struct back into a timestamp, assuming the input is in local time.
 */
time_t mktime(struct tm *timeptr) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

/**
 * Converts the given time struct back into a timestamp, assuming the input is in UTC.
 */
time_t timegm(struct tm *timeptr) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}
