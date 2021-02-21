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

// temporary stackule
static uintptr_t taskHandle = 0;

int Receiveboi(void *arg) {
    const uintptr_t handle = (uintptr_t) arg;

    ThreadSetName(0, "receiveyboi");
    LOG("handle: %08x'h", handle);

    int err;
    static char fuck[256] __attribute__((aligned(16)));
    memset(&fuck, 0, 256);

    ThreadUsleep(500 * 1000);

    while(1) {
        struct MessageHeader *msg = (struct MessageHeader *) &fuck;
        err = PortReceive(handle, msg, 256, UINTPTR_MAX);

        if(err > 0) {
            uint8_t *yeet = (uint8_t *) msg->data;
            LOG("PortReceive returned %d: from $%08x'h len %d bytes %02x %02x %02x %02x", 
                    err, msg->sender, msg->receivedBytes, yeet[0], yeet[1], yeet[2], yeet[3]);

            if(yeet[1] == 29) {
                goto fucking;
            }
        } else {
            LOG("PortReceive returned %d", err);
        }
    }

fucking:;
    LOG("aight im fuckin outta here");
    ThreadUsleep(1000 * 420);
    return 0x12344141;
}

/**
 * Configures our environment to be mostly sane.
 */
static void EnvInit() {
    // set up the task and thread names
    TaskSetName(0, "rootsrv");
    ThreadSetName(0, "Main");

    // retrieve task handle
    int err = TaskGetHandle(&taskHandle);
    REQUIRE(!err, "failed to get task handle: %d", err);
}

/**
 * Root server entry points
 *
 * We receive precisely no arguments. Yay!
 */
int main(int argc, const char **argv) {
    int err;

    // set up env; read the init bundle and init script
    EnvInit();

    task::Registry::init();
    task::RpcHandler::init();

    init::Bundle bundle;
    if(!bundle.validate()) {
        PANIC("failed to validate init bundle");
    }

    init::SetupServers(bundle);

    LOG("Malloc footprint %u", malloc_footprint());


    // create a port
    uintptr_t port;
    err = PortCreate(&port);
    REQUIRE(!err, "failed to create port: %d", err);

    LOG("Created port: $%08x'h", port);

    // send a bitchboi
    char cock[16] __attribute__((aligned(16)));
    memset(&cock, 0x41, 16);

    err = PortSend(port, &cock, 16);
    REQUIRE(!err, "failed to send to port: %d", err);

    err = PortSetQueueDepth(port, 5);
    REQUIRE(!err, "failed to set port queue depth: %d", err);

    // receiver boi
    thrd_t test;
    err = thrd_create(&test, Receiveboi, (void *) port);
    REQUIRE(err == thrd_success, "failed to create thread: %d", err);
    LOG("receiverboi thread $%p", test);

    for(size_t j = 0; j < 30; j++) {
        cock[0] = (j>>8) & 0xFF;
        cock[1] = j & 0xFF;

        //LOG("sending %u", j);
        again:;
        err = PortSend(port, &cock, 16);
        if(err) {
            LOG("failed to send to port: %d", err);

            if(err == -10) {
                ThreadUsleep(15000);
                goto again;
            }
        }

        ThreadUsleep(69 * 1000);
    }

    // start the message handler thread
    ThreadUsleep(69420 * 102);

    // test thread_join
    int cuck = 0;
    err = thrd_join(test, &cuck);
    REQUIRE(err == thrd_success, "failed to join thread: %d", err);
    LOG("Thread return: %08x (%d)", cuck, cuck);

    // launch all the loaded drivers

    // then the servers

    // wait for quit notification
    while(1) {
        ThreadUsleep(420 * 1000);
    }
}
