#include "Manager.h"
#include "LocalApicTimer.h"

#include <log.h>
#include <new>

using namespace platform::timer;

static uint8_t gSharedBuf[sizeof(Manager)] __attribute__((aligned(64)));
Manager *Manager::gShared = nullptr;

/**
 * Initializes the shared timer manager.
 */
void Manager::init() {
    gShared = reinterpret_cast<Manager *>(&gSharedBuf);
    new(gShared) Manager();
}



/**
 * Attempts to add a new timer.
 */
uintptr_t Manager::add(const uint64_t deadline, void (*callback)(const uintptr_t, void *),
        void *ctx) {
    // deadline mayn't be in the past
    if(deadline > now()) {
        return 0;
    }

    // build up a struct for it
    TimerInfo info;

    info.deadline = deadline;
    info.callback = callback;
    info.callbackCtx = ctx;

    // insert it
    RW_LOCK_WRITE_GUARD(this->timersLock);

    const auto id = this->nextTimerId++;
    info.token = id;

    this->timers.insert(id, info);
    return id;
}

/**
 * Removes a previously allocated timer, if it hasn't fired yet.
 */
void Manager::remove(const uintptr_t token) {
    RW_LOCK_WRITE_GUARD(this->timersLock);

    if(this->timers.contains(token)) {
        this->timers.remove(token);
    }
}



/**
 * Handles a tick of one of the underlying hardware timers.
 *
 * The first argument indicates the number of nanoseconds that this tick consists of; this is the
 * period of whatever timer invoked this. (This implies that you can only ever have one timer call
 * this method: this tracks since most of the time, that's the only timers we'll have!)
 */
void Manager::tick(const uint64_t ns, const uintptr_t irqToken) {
    // update the timer
    __atomic_fetch_add(&this->currentTime, ns, __ATOMIC_ACQUIRE);
    //const auto clock = now();

    // handle all software timers to see if any expired
    {
        RW_LOCK_READ_GUARD(this->timersLock);
    }
}



/**
 * Gets the current system timestamp.
 */
uint64_t platform_timer_now() {
    return Manager::now();
}

/**
 * Registers a new timer callback.
 */
uintptr_t platform_timer_add(const uint64_t at, void (*callback)(const uintptr_t, void *), void *ctx) {
    return Manager::gShared->add(at, callback, ctx);
}

/**
 * Removes a previously allocated timer, if it exists.
 */
void platform_timer_remove(const uintptr_t token) {
    return Manager::gShared->remove(token);
}
