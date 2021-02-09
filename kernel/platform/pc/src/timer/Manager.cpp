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
    if(deadline < now()) {
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
        const auto &timer = this->timers[token];
        if(timer.fired) return;

        this->timers.remove(token);
    }
}



/**
 * Handles a tick of one of the underlying hardware timers.
 *
 * The first argument indicates the number of nanoseconds that this tick consists of; this is the
 * period of whatever timer invoked this. (This implies that you can only ever have one timer call
 * this method: this tracks since most of the time, that's the only timers we'll have!)
 *
 * We don't call into the scheduler here; we assume that whoever invoked us will call that next and
 * whatever tasks became runnable will be scheduled.
 */
void Manager::tick(const uint64_t ns, const uintptr_t irqToken) {
    // update the timer
    __atomic_fetch_add(&this->currentTime, ns, __ATOMIC_ACQUIRE);
    const auto clock = now();

    // handle all software timers to see if any expired
    IterInfo info(clock);
    info.mgr = this;

    RW_LOCK_WRITE(&this->timersLock);

    this->timers.iterate([](void *ctx, const uintptr_t &key, TimerInfo &timer) {
        bool yes = true;
        bool remove = false;
        auto info = reinterpret_cast<IterInfo *>(ctx);

        if(timer.deadline <= info->time) {
            __atomic_store(&timer.fired, &yes, __ATOMIC_RELEASE);
            info->numFired++;

            // invoke the callback
            timer.callback(timer.token, timer.callbackCtx);

            // ensure it's removed
            remove = true;
        }

        return remove;
    }, &info);

    RW_UNLOCK_WRITE(&this->timersLock);

    // update scheduler if any timers happened
    if(info.numFired) {
        // TODO: do stuff
    }
}

/**
 * Returns the nanoseconds since the last time the current time variable was updated.
 */
uint64_t Manager::nsSinceTick() {
    return gShared->timebase->nsInTick();
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
