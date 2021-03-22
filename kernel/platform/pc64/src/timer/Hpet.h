#ifndef PLATFORM_PC64_TIMER_HPET_H
#define PLATFORM_PC64_TIMER_HPET_H

#include <stddef.h>
#include <stdint.h>

#include "Timer.h"

#include <platform.h>
#include <log.h>

namespace vm {
class MapEntry;
}

namespace platform {
/**
 * Implements the high-performance event timer, which is used as the global system timebase.
 */
class Hpet: public Timer {
    public:
        /// Reads ACPI tables and initialize the system HPET.
        static void Init();
        /// Return the system HPET
        static inline Hpet *the() {
            return gShared;
        }

        /// Converts timer ticks into nanoseconds
        uint64_t ticksToNs(const uint64_t ticks) const;
        /// Converts nanoseconds to timer ticks
        uint64_t nsToTicks(const uint64_t nsec, uint64_t &actualNsec) const;

        /// Performs a busy wait for the given time using the HPET as a reference.
        uint64_t busyWait(const uint64_t nsec);

        /// Returns the current timer value.
        inline uint64_t getCount() const {
            return *reinterpret_cast<volatile uint64_t *>(
                    reinterpret_cast<uintptr_t>(base) + kRegCount);
        }
        /// Returns ticks since the HPET was initialized.
        inline uint64_t ticksSinceInit() const {
            return this->getCount() - this->countOffset;
        }

    private:
        Hpet(const uintptr_t physBase, const void *table);
        ~Hpet();

        /// Writes a 64-bit HPET register.
        inline void write64(const size_t off, const uint64_t val) {
            REQUIRE(off <= this->largestOffset, "invalid HPET %s offset: %u", "write", off);

            auto ptr = reinterpret_cast<volatile uint64_t *>(
                    reinterpret_cast<uintptr_t>(base) + off);
            *ptr = val;
        }

        /// Reads a 64-bit HPET register.
        inline uint64_t read64(const size_t off) const {
            REQUIRE(off <= this->largestOffset, "invalid HPET %s offset: %u", "read", off);

            auto ptr = reinterpret_cast<volatile uint64_t *>(
                    reinterpret_cast<uintptr_t>(base) + off);
            return *ptr;
        }

    private:
        /// General capabilities & ID register
        constexpr static const size_t kRegGcId = 0x00;
        /// General configuration register
        constexpr static const size_t kRegGConf = 0x10;
        /// Current count register
        constexpr static const size_t kRegCount = 0xF0;

    private:
        static Hpet *gShared;

        /// whether HPET initialization is logged
        static bool gLogInit;

    private:
        /// HPET registers as mapped into VM space
        vm::MapEntry *vm = nullptr;

        /// Base address of HPET (in VM space)
        void *base = nullptr;
        /// Largest valid offset for a read/write
        size_t largestOffset = 0xFF8;

        /// number of comparators available to this HPET instance
        size_t numTimers = 0;
        /// period of a single clock tick, in femtoseconds
        uint64_t period = 0;

        /// HPET value at initialization
        uint64_t countOffset = 0;
};
}

#endif
