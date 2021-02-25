#include <stdio.h>
#include <stdlib.h>

#include <sys/syscalls.h>

#include "LaunchInfo.h"

/**
 * Initialization for the dynamic linker library
 */
void dyldo_start(kush_task_launchinfo_t *info) {
    // validate the info structure we got
    fprintf(stderr, "Launch info: %p\n", info);
    if(info->magic != TASK_LAUNCHINFO_MAGIC) {
        fprintf(stderr, "invalid task launchinfo magic: %08x\n", info->magic);
        abort();
    }

    fprintf(stderr, "fuck me (path '%s', %u args)\n", info->loadPath, info->numArgs);

    for(size_t i = 0; i < info->numArgs; i++) {
        fprintf(stderr, "Arg %u: '%s'\n", i, info->args[i]);
    }

    FILE *fucker = fopen("/bitch", "rb");
    if(fucker) {
        fclose(fucker);
    }

    while(1) {
        ThreadUsleep(1000 * 500);
    }
}

