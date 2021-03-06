#ifndef PLATFORM_PC64_IRQ_PIC_H
#define PLATFORM_PC64_IRQ_PIC_H

#include <stdint.h>

namespace platform::irq {
class LegacyPic {
    public:
        static void eoi(const uint8_t irq);
        static void disable();
};
}

#endif
