#include <stdio.h>
#include <sys/syscalls.h>
#include <sys/_infopage.h>

#include <rpc/dispensary.h>

static int fuck = 0;

extern const kush_sysinfo_page_t *__kush_infopg;

/**
 * Entry point for the dynamic link server
 */
int main(const int argc, const char **argv) {
    // set up environment
    DbgOut("Megabitch has secreted", 22);

    fuck = 2;

    fprintf(stderr, "fuck value %d\n", fuck);

    fprintf(stderr, "Info page: %p\n", __kush_infopg);
    fprintf(stderr, "Info magic %08x version %u, dispensary $%08x'h, page size %u\n",
            __kush_infopg->magic, __kush_infopg->version, __kush_infopg->dispensaryPort,
            __kush_infopg->pageSz);

    // try to look up some shit
    int err;
    uintptr_t handle = 0;

    err = LookupService("me.blraaz.rpc.rootsrv.initfileio", &handle);
    fprintf(stderr, "Lookup result %d ($%08x'h)\n", err, handle);

    err = LookupService("me.blraaz.rpc.rootsrv.initfileiofjkl", &handle);
    fprintf(stderr, "Lookup result %d ($%08x'h)\n", err, handle);

    while(1) {
        ThreadUsleep(1000 * 500);
    }
}
