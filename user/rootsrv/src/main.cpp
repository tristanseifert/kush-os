#include <sys/syscalls.h>
#include <sys/x86/syscalls.h>

#include <memory>

#include "init/Init.h"
#include "init/Bundle.h"
#include "init/BundleFileRpcHandler.h"
#include "task/Registry.h"
#include "task/RpcHandler.h"
#include "task/InfoPage.h"
#include "dispensary/Dispensary.h"

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
 * Loads the init bundle and sets up the file RPC provider. Then, load all servers requested.
 */
static void BundleInit() {
    // load bundle
    auto bundle = std::make_shared<init::Bundle>();
    if(!bundle->validate()) {
        PANIC("failed to validate init bundle");
    }

    init::BundleFileRpcHandler::init(bundle);

    // initialize the servers we've loaded from the init script
    init::SetupServers(bundle);
}

/**
 * Root server entry points
 *
 * We receive precisely no arguments. Yay!
 */
int main(int argc, const char **argv) {
    // set up our environment and core structures
    EnvInit();

    task::InfoPage::init();

    dispensary::init();
    task::Registry::init();

    // start RPC handlers
    task::RpcHandler::init();

    // load bundle and the associated servers
    fprintf(stderr, "starting bundle init\n");
    BundleInit();

    // wait for quit notification
    while(1) {
        ThreadUsleep(420 * 1000);
    }
}
