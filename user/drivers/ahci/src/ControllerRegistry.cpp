#include "ControllerRegistry.h"

#include "Log.h"

ControllerRegistry *ControllerRegistry::gShared{nullptr};

/**
 * Initializes the controller registry.
 */
void ControllerRegistry::init() {
    if(gShared) Abort("Cannot reinitialize controller registry");
    gShared = new ControllerRegistry;
}

/**
 * Adds a new device to the controller registry.
 */
void ControllerRegistry::addController(const std::shared_ptr<Controller> &controller) {
    this->controllers.emplace_back(controller);
}
