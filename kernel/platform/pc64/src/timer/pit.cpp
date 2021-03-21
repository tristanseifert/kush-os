#include "pit.h"

#include <stddef.h>
#include <log.h>
#include <arch/x86_io.h>

using namespace platform;

bool LegacyPIT::gLogConfig = false;

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

