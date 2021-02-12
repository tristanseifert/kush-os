#include "Syscall.h"
#include "Handlers.h"

#include "sched/Thread.h"

#include <new>
#include <log.h>

using namespace sys;

static uint8_t gSharedBuf[sizeof(Syscall)] __attribute__((aligned(64)));
Syscall *Syscall::gShared = nullptr;

/**
 * Global syscall table
 */
static int (* const gSyscalls[])(const Syscall::Args *, const uintptr_t) = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

    // 0x10: yield remainder of CPU time
    [](auto, auto) {
        sched::Thread::yield();
        return 0;
    },
    // 0x11: sleeps the thread for the number of microseconds in arg0
    [](auto args, auto) {
        // validate args
        const uint64_t sleepNs = 1000ULL * args->args[0];
        if(!sleepNs) return 0;
        // call sleep handler
        sched::Thread::sleep(sleepNs);
        return 0;
    },

    // 0x32: Initialize task
    TaskInitialize,
};
static const size_t gNumSyscalls = 32;

/**
 * Initializes the syscall handler.
 */
void Syscall::init() {
    gShared = reinterpret_cast<Syscall *>(&gSharedBuf);
    new(gShared) Syscall();
}

/**
 * Handles a syscall that wasn't handled by fast path code, nor platform or architecture code.
 */
int Syscall::_handle(const Args *args, const uintptr_t code) {
    // validate syscall number
    const size_t syscallIdx = (code & 0xFFFF);
    if(syscallIdx >= gNumSyscalls) {
        return -1;
    }

    // invoke it
    return gSyscalls[syscallIdx](args, code);
}
