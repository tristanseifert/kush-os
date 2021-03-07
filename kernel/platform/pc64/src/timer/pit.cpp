#include "pit.h"

#include <stddef.h>
#include <log.h>
#include <arch/x86_io.h>

using namespace platform::timer;

/**
 * Disables the legacy 8254 programmable timer. Most BIOSes will configure this on bootup to be
 * enabled on channel 0.
 */
void LegacyPIT::disable() {
    // put channel 0 into SW triggered strobe; lobyte/hibyte access
    io_outb(kCommandPort, 0b00111000);

    //io_outb(kCh0DataPort, 0xFF);
    //io_outb(kCh0DataPort, 0xFF);
}

/**
 * Configures the PIT for a busy wait. This can take some extra time, and we'll only usually use
 * the busy waiting to measure other timers, so this is done in a separate function call to avoid
 * diluting the results.
 *
 * Since the PIT has a royally fucked input clock, we have to do some wonky math to get a frequency
 * that is closest to what's requested; we output that to the given variable.
 *
 * To do this, we use the HW retriggerable one shot mode of the timer. The timer will begin to
 * count on a rising edge on the gate input, decrementing it from the reload value set at a rate
 * of 1.193182 MHz, until it eventually decrements to zero.
 *
 * @return Number of picoseconds that we actually slept for
 */
uint64_t LegacyPIT::configBusyWait(const uint32_t micros) {
    uint16_t reload = 0xFFFF;

    // calculate the reload value
    static const double nsPerCount = 838.0953445668707;
    const double desiredNs = ((double) micros) * 1000.;

    const auto count = desiredNs / nsPerCount;
    const auto intCount = (uint32_t) (count);

    const auto actualMicros = (nsPerCount * intCount) / 1000.;

    //log("count %g int %lu actual time %g", count, intCount, actualMicros);
    REQUIRE(intCount <= 0xFFFF, "frequency too high");
    reload = intCount;

    // set gate input low
    const auto temp = io_inb(kTimerIoPort);
    io_outb(kTimerIoPort, temp & ~kCh2GateBit);

    // configure channel 2 as a HW retriggerable one shot
    io_outb(kCommandPort, 0b10110010);

    // write reload register
    io_outb(kCh2DataPort, (reload & 0xFF));
    io_outb(kCh2DataPort, (reload & 0xFF00) >> 8);

    // return the actual number of microseconds we'll sleep
    return static_cast<uint64_t>(actualMicros * 1000. * 1000.);
}


/**
 * Performs the previously configured busy wait.
 */
void LegacyPIT::busyWait() {
    uint16_t count = 0, lastCount = 0;
    size_t iterations = 0;

    // start timer 2 (put a rising edge on the gate input)
    const auto temp = io_inb(kTimerIoPort);
    io_outb(kTimerIoPort, temp | kCh2GateBit);
    io_outb(kTimerIoPort, temp & ~kCh2GateBit);

    // read it until it expires
    while(true) {
        // read back command: channel 2, latch count
        io_outb(kCommandPort, 0b11011000);

        count = io_inb(kCh2DataPort);
        count |= ((uint16_t) io_inb(kCh2DataPort)) << 8;

        // make sure we haven't missed a wrap around
        if(count > lastCount) {
            iterations++;
            if(iterations >= 2) {
                log("PIT timing missed loop!");
            }
        }
        lastCount = count;

        // exit if the count is 0 or 0xFFFF
        if(!count || count == 0xFFFF) return;
    }

    // disable the channel
    io_outb(kCommandPort, 0b10111000);
}
