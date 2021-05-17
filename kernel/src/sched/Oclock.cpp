#include "Oclock.h"

#include <log.h>
#include <platform.h>

using namespace sched;

/**
 * Starts a particular timer.
 *
 * @return The timer that was running immediately before the new timer was started.
 */
Oclock::Type Oclock::start(const Type t) {
    // get the type of the currently running timer and stop it
    Type current = this->active;
    if(current != Type::None) {
        this->stop(current);
    }

    // starting a None timer is a no-op, but will stop the currently running timer
    if(t == Type::None) return Type::None;

    // store start timestamp and update state
    this->startTimes[static_cast<size_t>(t)] = platform::GetLocalTsc();
    this->active = t;

    return current;
}

/**
 * Stops the given timer, and returns its current accumulator value.
 */
uint64_t Oclock::stop(const Type t) {
    if(t == Type::None) return 0;

    // stop the thymer
    const auto stop = platform::GetLocalTsc();
    const auto start = this->startTimes[static_cast<size_t>(t)];
    const auto delta = stop - start;

    this->active = Type::None;

    // update accumulator
    return __atomic_add_fetch(&this->accumulator[static_cast<size_t>(t)], delta, __ATOMIC_RELEASE);
}
