#include <sys/syscalls.h>
#include <sys/x86/syscalls.h>
#include <cstdint>
#include <cstring>
#include <assert.h>

#include <vector>

#include "init/Bundle.h"

#include "log.h"

// temporary stackule
static uint8_t fuck[1024];

static uintptr_t taskHandle = 0;

void Receiveboi(const uintptr_t handle) {
    ThreadSetName(0, "receiveyboi");
    LOG("handle: %08x'h", handle);

    int err;
    char fuck[256] __attribute__((aligned(16)));
    memset(&fuck, 0, 256);

    ThreadUsleep(500 * 1000);

    while(1) {
        struct MessageHeader *msg = (struct MessageHeader *) &fuck;
        err = PortReceive(handle, msg, 256, UINTPTR_MAX);

        if(err > 0) {
            uint8_t *yeet = (uint8_t *) msg->data;
            LOG("PortReceive returned %d: from $%08x'h len %d bytes %02x %02x %02x %02x", 
                    err, msg->sender, msg->receivedBytes, yeet[0], yeet[1], yeet[2], yeet[3]);
        } else {
            LOG("PortReceive returned %d", err);
        }
    }
}

int main(int argc, const char **argv) {
    int err;
    uintptr_t threadHandle = 0, newThread = 0;
    memset(fuck, 0xee, 1024);

    // initialize environment
    TaskSetName(0, "rootsrv");
    ThreadSetName(0, "Main");

    err = TaskGetHandle(&taskHandle);
    REQUIRE(!err, "failed to get task handle: %d", err);
    err = ThreadGetHandle(&threadHandle);
    REQUIRE(!err, "failed to get thread handle: %d", err);

    LOG("task handle %08x; thread handle %08x", taskHandle, threadHandle);

    // parse the init bundle and read the init script
    init::Bundle bundle;
    if(!bundle.validate()) {
        PANIC("failed to validate init bundle");
    }

    auto scriptFile = bundle.open("/etc/default.init");
    REQUIRE(scriptFile, "failed to open default init script");

    LOG("Init script: %u bytes, '%s'", scriptFile->getSize(), scriptFile->getContents().data());

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

    /*
    // vector test
    std::vector<int> balls;
    balls.push_back(420);
    balls.push_back(69);

    for(auto fuck : balls) {
        LOG("fucker %d", fuck);
    }
*/

    // receiver boi
    err = ThreadCreate(Receiveboi, port, ((uintptr_t) &fuck) + 1024, &newThread);
    REQUIRE(!err, "failed to create thread: %d", err);
    LOG("receiverboi thread handle $%08x'h", newThread);

    for(size_t j = 0; j < 420; j++) {
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
    err = ThreadDestroy(newThread);
    LOG("ThreadDestroy: %d", err);

    // launch all the loaded drivers

    // then the servers

    // wait for quit notification
    while(1) {
        ThreadUsleep(420 * 1000);
    }
}
