#ifndef PLATFORM_PC_TIMER_MANAGER_H
#define PLATFORM_PC_TIMER_MANAGER_H

#include <platform.h>

#include <stddef.h>
#include <stdint.h>

#include <arch/rwlock.h>
#include <runtime/HashTable.h>

namespace platform {
namespace irq {
class Manager;
}
namespace timer {
class LocalApicTimer;

/**
 * Provides a sort of HAL interface around the system's timers.
 *
 * Callers may register functions to be invoked at a certain time in the future. The timer is
 * yeeted into a list of timers, and will be signaled when its time has elapsed. If the caller no
 * longer wants to receive the timer, it can cancel it; but once started, a timer may not be
 * modified.
 *
 * Internally, we keep a time counter that tracks the number of nanoseconds since the system was
 * booted. Time is tracked as a 64-bit number of nanoseconds, which gives us several hundred years
 * of resolution: i.e. way longer than this code will be around for for...
 */
class Manager {
    friend void ::platform_vm_available();
    friend unsigned int ::platform_timer_add(const uint64_t, void (*cb)(const uintptr_t, void *), void *);
    friend void ::platform_timer_remove(const uintptr_t);
    friend class irq::Manager;

    public:
        /// Returns the current timestamp value
        static inline const uint64_t now() {
            uint64_t temp;
            __atomic_load(&gShared->currentTime, &temp, __ATOMIC_CONSUME);
            return temp + nsSinceTick();
        }

        /// Registers a new timer
        uintptr_t add(const uint64_t at, void (*callback)(const uintptr_t, void *), void *ctx);
        /// Removes a timer, if possible
        void remove(const uintptr_t token);

    private:
        /// Info on a registered timer
        struct TimerInfo {
            /// deadline
            uint64_t deadline;
            /// token for the timer
            uintptr_t token;

            /// when set, the timer has fired
            bool fired = false;

            /// function to invoke
            void (*callback)(const uintptr_t, void *);
            /// context to callback
            void *callbackCtx = nullptr;
        };

        /// Info for iterating
        struct IterInfo {
            const uint64_t time;
            Manager *mgr = nullptr;

            /// number of timers that fired during this iteration
            size_t numFired = 0;

            IterInfo(const uint64_t clock) : time(clock) {}
        };

        friend const uint32_t rt::hash(TimerInfo &in, const uint32_t);

    private:
        static void init();
        static uint64_t nsSinceTick();

        void tick(const uint64_t ns, const uintptr_t irqToken);

        static Manager *gShared;

    private:
        /// number of nanoseconds since system boot-up
        uint64_t __attribute__((aligned(8))) currentTime = 0;
        /// Timebase we're using
        LocalApicTimer *timebase = nullptr;

        /// next free timer value (TODO: handle rollover)
        uintptr_t nextTimerId = 1;

        /// lock over the timers
        DECLARE_RWLOCK(timersLock);
        /// all assigned timers
        rt::HashTable<uintptr_t, TimerInfo> timers;
};
}}

template<>
inline const uint32_t rt::hash(platform::timer::Manager::TimerInfo &in, const uint32_t seed) {
    return in.token;
}

#endif
