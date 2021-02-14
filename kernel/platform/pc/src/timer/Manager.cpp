#include "Manager.h"
#include "LocalApicTimer.h"

#include <log.h>
#include <new>

#include <arch/critical.h>

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
    DECLARE_CRITICAL();

    // build up a struct for it
    TimerInfo info;

    info.deadline = deadline;
    info.callback = callback;
    info.callbackCtx = ctx;

    // insert it
    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->timersLock);

again:;
    const auto id = this->nextTimerId++;
    if(!id) goto again;
    info.token = id;

    this->timers.append(info);

    RW_UNLOCK_WRITE(&this->timersLock);
    CRITICAL_EXIT();

    return id;
}

/**
 * Removes a previously allocated timer, if it hasn't fired yet.
 */
void Manager::remove(const uintptr_t token) {
    DECLARE_CRITICAL();

    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->timersLock);

    for(size_t i = 0; i < this->timers.size(); i++) {
        const auto &timer = this->timers[i];
        if(timer.token == token) {
            this->timers.remove(i);
            goto yeet;
        }
    }

yeet:;
    RW_UNLOCK_WRITE(&this->timersLock);
    CRITICAL_EXIT();
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
void Manager::tick(const uint64_t ns) {
    // update the clock value
    __atomic_fetch_add(&this->currentTime, ns, __ATOMIC_ACQUIRE);
}

/**
 * Invokes handlers for any timers that have expired.
 */
void Manager::checkTimers() {
    REQUIRE_IRQL_LEQ(platform::Irql::Scheduler);
    const auto clock = now();

    // handle all software timers to see if any expired
    RW_LOCK_WRITE(&this->timersLock);

    IterInfo info(clock);
    info.mgr = this;

    this->timers.removeMatching([](void *ctx, TimerInfo &timer) {
        bool yes = true;
        auto info = reinterpret_cast<IterInfo *>(ctx);

        if(timer.deadline <= info->time) {
            __atomic_store(&timer.fired, &yes, __ATOMIC_RELEASE);
            timer.callback(timer.token, timer.callbackCtx);

            return true;
        }

        return false;
    }, &info);

    RW_UNLOCK_WRITE(&this->timersLock);
}

/**
 * Returns the nanoseconds since the last time the current time variable was updated.
 */
uint64_t Manager::nsSinceTick() {
    if(!gShared->timebase) return 0;
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
