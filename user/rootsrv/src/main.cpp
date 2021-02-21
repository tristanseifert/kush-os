#include <sys/syscalls.h>
#include <sys/x86/syscalls.h>
#include <cstdint>
#include <cstring>
#include <malloc.h>
#include <threads.h>

#include "init/Init.h"
#include "init/Bundle.h"
#include "task/Registry.h"
#include "task/RpcHandler.h"

#include "log.h"

uintptr_t gTaskHandle = 0;

/**
 * Configures our environment to be mostly sane.
 */
static void EnvInit() {
    // set up the task and thread names
    TaskSetName(0, "rootsrv");
    ThreadSetName(0, "Main");

    // retrieve task handle
    int err = TaskGetHandle(&gTaskHandle);
    REQUIRE(!err, "failed to get task handle: %d", err);
}

/**
 * Root server entry points
 *
 * We receive precisely no arguments. Yay!
 */
int main(int argc, const char **argv) {
    // set up env; read the init bundle and init script
    EnvInit();

    task::Registry::init();
    task::RpcHandler::init();

    init::Bundle bundle;
    if(!bundle.validate()) {
        PANIC("failed to validate init bundle");
    }


    // initialize the servers we've loaded from the init script
    init::SetupServers(bundle);

    // wait for quit notification
    while(1) {
        ThreadUsleep(420 * 1000);
    }
}
