#include <cstdio>
#include <cstdlib>

#include <sys/syscalls.h>
#include <malloc.h>

#include "Linker.h"
#include "LaunchInfo.h"

using namespace dyldo;

/**
 * Initialization for the dynamic linker library
 */
extern "C" void dyldo_start(const kush_task_launchinfo_t *info) {
    // validate the info structure we got
    if(info->magic != TASK_LAUNCHINFO_MAGIC) {
        fprintf(stderr, "invalid task launchinfo magic: %08x\n", info->magic);
        abort();
    }

    // handle the linking process
    Linker::init(info->loadPath);

    Linker::Trace("Loading libraries");
    Linker::the()->loadLibs();
    Linker::Trace("Fixing up segments");
    Linker::the()->doFixups();

    // clean up and prepare to jump to program entrypoint
    Linker::Trace("Cleaning up");
    Linker::the()->cleanUp();
    malloc_trim(0);

    // XXX: debug log
    const auto maxAlloc = malloc_max_footprint();
    Linker::Trace("Max alloc: %u bytes", maxAlloc);

    // invoke initializers, then invoke entry point
    Linker::Trace("Invoking initializers");
    Linker::the()->runInit();
    Linker::Trace("Jump to program entry point (info %p)", info);
    Linker::the()->jumpToEntry(info);
}

