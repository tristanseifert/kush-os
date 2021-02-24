#include <unistd.h>
#include <errno.h>

#include <sys/_infopage.h>

extern const kush_sysinfo_page_t *__kush_infopg;

/**
 * Implements the sysconf interface to get some system configuration details.
 *
 * This is, for the most part, implemented in the gigantic switch statement below. Most of the
 * variables are either hard-coded here, or we pull out of the system info page.
 */
long sysconf(int name) {
    switch(name) {
        // physical page size
        case _SC_PAGESIZE:
            return __kush_infopg->pageSz;

        // unknown
        default:
            errno = EINVAL;
            return -1;
    }
}
