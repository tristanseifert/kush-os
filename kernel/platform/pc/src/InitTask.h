#ifndef PLATFORM_PC_INITTASK_H
#define PLATFORM_PC_INITTASK_H

#include <stdint.h>

namespace platform {
void InitHandleModule(const uint32_t type, const uint32_t physBase, const uint32_t physEnd,
        const char *cmdline);

}

#endif
