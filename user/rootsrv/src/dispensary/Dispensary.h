#ifndef DISPENSARY_DISPENSARY_H
#define DISPENSARY_DISPENSARY_H

#include <memory>
#include <string_view>

namespace dispensary {
/**
 * Initializes the dispensary.
 */
void init();

/**
 * Registers a port to the dispensary.
 */
void RegisterPort(const std::string_view &name, const uintptr_t handle);

/**
 * Unrgisters a port to the dispensary.
 */
void UnregisterPort(const std::string_view &name);
}

#endif
