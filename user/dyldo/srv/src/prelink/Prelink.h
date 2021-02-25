#ifndef PRELINK_PRELINK_H
#define PRELINK_PRELINK_H

#include <cstdint>

namespace prelink {
/**
 * Performs the entire prelink process of system libraries.
 */
void Load();

/**
 * Sets up prelink libraries for the given task, and returns where the dynamic linker entry point
 * stub is mapped.
 */
void BootstrapTask(const uintptr_t taskHandle, uintptr_t &outEntry);
}

#endif
