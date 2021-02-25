#include <cstdio>
#include <cstdlib>

#include <sys/syscalls.h>
#include <malloc.h>

#include "Linker.h"
#include "LaunchInfo.h"

using namespace dyldo;

static Linker *gLinker = nullptr;

/**
 * Initialization for the dynamic linker library
 */
extern "C" void dyldo_start(kush_task_launchinfo_t *info) {
    // validate the info structure we got
    fprintf(stderr, "Launch info: %p\n", info);
    if(info->magic != TASK_LAUNCHINFO_MAGIC) {
        fprintf(stderr, "invalid task launchinfo magic: %08x\n", info->magic);
        abort();
    }

    // handle the linking process
    gLinker = new Linker(info->loadPath);
    if(!gLinker) {
        fprintf(stderr, "out of memory\n");
        abort();
    }

    gLinker->loadLibs();

    // clean up and prepare to jump to program entrypoint
    gLinker->cleanUp();
    malloc_trim(0);

    // jump to program's actual entry point (passing along the info struct ptr)
    fprintf(stderr, "fuck me (path '%s', %u args)\n", info->loadPath, info->numArgs);

    for(size_t i = 0; i < info->numArgs; i++) {
        fprintf(stderr, "Arg %u: '%s'\n", i, info->args[i]);
    }

    // XXX: debug log
    const auto maxAlloc = malloc_max_footprint();
    fprintf(stdout, "Max alloc: %u bytes\n", maxAlloc);

    while(1) {
        ThreadUsleep(1000 * 500);
    }
}

