#include <platform.h>
#include <log.h>

#include <sched/Task.h>
#include <sched/Thread.h>
#include <vm/Map.h>
#include <vm/MapEntry.h>

/**
 * Loads the root server binary from the ramdisk.
 */
sched::Task *platform_init_rootsrv() {
    panic("%s unimplemented", __PRETTY_FUNCTION__);
}
