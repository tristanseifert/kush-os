#ifndef KERNEL_SCHED_OCLOCK_H
#define KERNEL_SCHED_OCLOCK_H

#include <cstddef>
#include <cstdint>

namespace sched {
/**
 * Provides a stopwatch-like interface used for time accounting on a particular CPU core. It
 * has several distinct "types" of stopwatches, of which only one can be running at a time.
 *
 * Various sections of code can start a new type of time accounting region and restore the previous
 * type after, forming a sort of stack. This allows the time quantum of a thread executing to be
 * counted without including IRQs, for example.
 *
 * All time values are returned in nanoseconds.
 */
class Oclock {
    constexpr static const size_t kNumTimers = 3;

    public:
        enum class Type {
            /// Thread executing (kernel mode)
            ThreadKernel        = 0,
            /// Thread executing (user mode)
            ThreadUser          = 1,
            /// IRQ handler
            Interrupt           = 2,

            /// total number of types
            NumTypes,

            /// No timer is running
            None,
        };
        static_assert(static_cast<size_t>(Type::NumTypes) <= kNumTimers);

    public:
        /// Start a particular stop watch
        Type start(const Type t);
        /// Stop the currently running timer, and return its accumulator value
        uint64_t stop() {
            return this->stop(this->active);
        }

        /// Get the value of the given stopwatch
        inline uint64_t get(const Type t) const {
            return __atomic_load_n(&this->accumulator[static_cast<size_t>(t)], __ATOMIC_RELAXED);
        }
        /// Reset the stop watch with the given type and return its value
        inline uint64_t reset(const Type t) {
            return __atomic_exchange_n(&this->accumulator[static_cast<size_t>(t)], 0,
                    __ATOMIC_RELEASE);
        }

    private:
        uint64_t stop(const Type t);

    private:
        /// nanosecond counters (accumulators) for each type
        uint64_t accumulator[kNumTimers];
        /// timestamps at which a particular stopwatch was started
        uint64_t startTimes[kNumTimers];

        /// currently active stopwatch
        Type active = Type::None;
};
}

#endif
