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
 * Shuts down the controller registry. This will drop our references to the controllers, which
 * will result in them being deallocated.
 */
void ControllerRegistry::deinit() {
    if(!gShared) Abort("Cannot deinitialize an uninitialized controller registry");
    delete gShared;
    gShared = nullptr;
}

/**
 * Adds a new device to the controller registry.
 */
void ControllerRegistry::addController(const std::shared_ptr<Controller> &controller) {
    this->controllers.emplace_back(controller);
}
