#include "Dispensary.h"
#include "Registry.h"
#include "RpcHandler.h"

#include "log.h"

#include <system_error>

using namespace dispensary;

/**
 * Initializes the dispensary service.
 */
void dispensary::init() {
    Registry::init();
    RpcHandler::init();
}

/**
 * Register a port.
 */
void dispensary::RegisterPort(const std::string_view &name, const uintptr_t handle) {
    Registry::gShared->registerPort(name, handle);
}

/**
 * Unregister a port.
 */
void dispensary::UnregisterPort(const std::string_view &name) {
    Registry::gShared->unregisterPort(std::string(name));
}

