#ifndef PLATFORM_PC_TIMER_PIT_H
#define PLATFORM_PC_TIMER_PIT_H

#include <stdint.h>

namespace platform { namespace timer {
/**
 * Provides a very basic intercace to the legacy PIT on the PC platform.
 *
 * We don't actually use it (because it's kind of shitty) so we really only have enough logic to
 * be able to disable it.
 */
class LegacyPIT {
    private:
        /// Channel 0 data port
        constexpr static const uint16_t kCh0DataPort = 0x40;
        /// Channel 1 data port
        constexpr static const uint16_t kCh1DataPort = 0x41;
        /// Channel 2 data port
        constexpr static const uint16_t kCh2DataPort = 0x42;
        /// IO port address of the PIT command port
        constexpr static const uint16_t kCommandPort = 0x43;

        /// IO port for the timer IOs
        constexpr static const uint16_t kTimerIoPort = 0x61;
        /// Bit for the channel 2 gate output
        constexpr static const uint8_t kCh2GateBit = (1 << 0);

    private:
        /// whether re-configurations of the PIT are logged
        static bool gLogConfig;

    public:
        static void disable();

        /// Returns the number of picoseconds the actual wait was for
        static uint64_t configBusyWait(const uint64_t micros);
        static bool busyWait();
};
}}

#endif
