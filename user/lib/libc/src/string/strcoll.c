#include <sys/cdefs.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wchar.h>

int strcoll(const char *s1, const char *s2) {
    /* LC_COLLATE is unimplemented, hence always "C" */
    return (strcmp(s1, s2));
}
