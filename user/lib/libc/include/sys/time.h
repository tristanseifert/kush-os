#ifndef _SYS_TIME_H_
#define	_SYS_TIME_H_

#include <_libc.h>
#include <time.h>

#ifndef _SUSECONDS_T_DECLARED
typedef    int    suseconds_t;
#define    _SUSECONDS_T_DECLARED
#endif

/// XXX: do we need to define this properly at some point
struct timezone;

struct timeval {
    time_t      tv_sec;     /* seconds */
    suseconds_t tv_usec;    /* microseconds */
};

#ifdef __cplusplus
extern "C" {
#endif

LIBC_EXPORT int gettimeofday(struct timeval *tv, struct timezone *tz);

#ifdef __cplusplus
}
#endif

#endif
