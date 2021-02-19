#include <sys/syscalls.h>
#include <sys/x86/syscalls.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <vector>

#include "log.h"

// temporary stackule
static uint8_t fuck[1024];

static uintptr_t taskHandle = 0;

static inline void io_outb(const uint16_t port, const uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

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

void Kugelschreiber(const uintptr_t fuck) {
    ThreadSetName(0, "megafgt deluxe");

    uintptr_t handle = 0;
    int err = ThreadGetHandle(&handle);
    REQUIRE(!err, "failed to get thread handle: %d", err);

    LOG("bitchass %08x (thread handle %08x)", fuck, handle);

    // try to test map VGA memory
    uintptr_t region = 0;
    err = AllocVirtualRegion(0xb8000, 0x20000000, 0x8000, VM_REGION_RW | VM_REGION_NOMAP, &region);
    REQUIRE(!err, "failed to create VM region: %d", err);

    LOG("VM region handle: %08x", region);

    err = MapVirtualRegionAt(region, 0x30000000);
    REQUIRE(!err, "failed to map VM region: %d", err);

    *((uint16_t *) 0x30000420) = 0x0E00 + 'w';

    // get region info
    uintptr_t base, length, flags;
    err = VirtualRegionGetInfo(region, &base, &length, &flags);
    REQUIRE(!err, "failed to get VM region info: %d", err);

    LOG("VM region $%08x'h base $%08x, length $%08x flags $%08x", region, base, length, flags);

    // unmap and get info again
    err = UnmapVirtualRegionFrom(region, taskHandle);
    REQUIRE(!err, "failed to unmap VM region: %d", err);

    uint8_t iopb[64];
    // memset(&iopb, 0xff, 64);
    iopb[16] = 0b00000001;

    err = X86UpdateIopb(iopb, 512, 0);
    REQUIRE(!err, "failed to update IOPB: %d", err);

    io_outb(0x80, 0x33);


assert(0);
    while(1) {
        ThreadUsleep(6940 * 100);
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

    // parse the init bundle

    // test the anon shit
    uintptr_t region = 0, region2 = 0;
    err = AllocVirtualAnonRegion(0/*x28000000*/, 0x10000, VM_REGION_RW, &region);
    REQUIRE(!err, "failed to allocate anon region: %d", err);

    uintptr_t base, length, flags;
    err = VirtualRegionGetInfo(region, &base, &length, &flags);
    REQUIRE(!err, "failed to get VM region info: %d", err);

    LOG("anon region base %08x", base);

    uintptr_t *regionPtr = (uintptr_t *) base;
    regionPtr[0] = 0xDEADBEEF;
    regionPtr[0x2000] = 0xDEADBEEF;
    LOG("region %08x %08x %08x %08x", regionPtr[0], regionPtr[1], regionPtr[0x1000], regionPtr[0x2000]);

    // try to allocate more shit
    err = AllocVirtualAnonRegion(0, 0x100000, VM_REGION_RW, &region2);
    REQUIRE(!err, "failed to allocate anon region 2: %d", err);
    err = VirtualRegionGetInfo(region2, &base, &length, &flags);
    REQUIRE(!err, "failed to get VM region info 2: %d", err);

    LOG("region2 @ %08x", base);

    regionPtr = (uintptr_t *) base;
    regionPtr[0] = 0xDEADBEEF;
    regionPtr[0x2000] = 0xDEADBEEF;
    LOG("region %08x %08x %08x %08x", regionPtr[0], regionPtr[1], regionPtr[0x1000], regionPtr[0x2000]);

    err = UnmapVirtualRegion(region2);
    REQUIRE(!err, "failed to unmap anon region2: %d", err);
    err = UnmapVirtualRegion(region);
    REQUIRE(!err, "failed to unmap anon region: %d", err);

    //err = ThreadCreate(Kugelschreiber, 0xDEADBEEF, ((uintptr_t) &fuck) + 1024, &newThread);
    //REQUIRE(!err, "failed to create thread: %d", err);
    // LOG("new thread %08x", newThread);

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
     LOG("receiverboi thread %08x", newThread);

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
