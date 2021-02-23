#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscalls.h>
#include <sys/_infopage.h>

#include <rpc/file.h>
#include <rpc/dispensary.h>

static int fuck = 0;

extern kush_sysinfo_page_t *__kush_infopg;

/**
 * Entry point for the dynamic link server
 */
int main(const int argc, const char **argv) {
    int err;

    // set up environment
    DbgOut("Megabitch has secreted", 22);

    fuck = 2;

    fprintf(stderr, "fuck value %d\n", fuck);

    fprintf(stderr, "Info page: %p\n", __kush_infopg);
    fprintf(stderr, "Info magic %08x version %u, dispensary $%08x'h, page size %u\n",
            __kush_infopg->magic, __kush_infopg->version, __kush_infopg->dispensaryPort,
            __kush_infopg->pageSz);

    // resolve the region the info page is in 
    uintptr_t vmHandle, base, len, flags;
    const auto addr =(uintptr_t) &__kush_infopg; 
    err = VirtualGetHandleForAddr(addr, &vmHandle);
    fprintf(stderr, "Resolving addr $%08x -> $%08x'h %d\n", addr, vmHandle, err);
    assert(err == 1);

    fprintf(stderr, "Info pg handle $%08x'h\n", vmHandle);

    err = VirtualRegionGetInfo(vmHandle, &base, &len, &flags);

    fprintf(stderr, "Info pg base $%08x len $%0x flags $%08x\n", base, len, flags);

    /*
    // try to look up some shit

    err = LookupService("me.blraaz.rpc.rootsrv.initfileio", &handle);
    fprintf(stderr, "Lookup result %d ($%08x'h)\n", err, handle);

    err = LookupService("me.blraaz.rpc.rootsrv.initfileiofjkl", &handle);
    fprintf(stderr, "Lookup result %d ($%08x'h)\n", err, handle);
*/

    // try file stuff
    void *read = malloc(42069);
    assert(read);

/*
    uintptr_t handle = 0;
    uint64_t fileSz;
    err = FileOpen("/lib/libdyldo.so", FILE_OPEN_READ, &handle, &fileSz);
    fprintf(stderr, "FileOpen(): %d, handle $%08x length %llu bytes\n", err, handle, fileSz);

    err = FileRead(handle, 0, 42069, read);
    fprintf(stderr, "FileRead(): %d\n", err);

    err = FileClose(handle);
    fprintf(stderr, "FileClose(): %d\n", err);
*/

    FILE *fuck = fopen("/lib/libdyldo.so", "rb");
    fprintf(stderr, "fopen(): %p %d\n", fuck, errno);

    long pos = ftell(fuck);
    fprintf(stderr, "ftell(): %ld\n", pos);

    err = fseek(fuck, 0, SEEK_END);
    assert(!err && "fseek");
    pos = ftell(fuck);
    fprintf(stderr, "ftell(): %ld\n", pos);
    err = fseek(fuck, 0, SEEK_SET);
    assert(!err && "fseek");
    pos = ftell(fuck);
    fprintf(stderr, "ftell(): %ld\n", pos);

    err = fread(read, 42069, 1, fuck);
    fprintf(stderr, "fread(): %d\n", err);

    pos = ftell(fuck);
    fprintf(stderr, "ftell(): %ld\n", pos);

    err = fclose(fuck);
    fprintf(stderr, "fclose(): %d\n", err);

    free(read);

    while(1) {
        ThreadUsleep(1000 * 500);
    }
}
