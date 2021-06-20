#ifndef AHCIDRV_CONTROLLERREGISTRY_H
#define AHCIDRV_CONTROLLERREGISTRY_H

#include <memory>
#include <vector>

class Controller;

class ControllerRegistry {
    public:
        static void init();
        static void deinit();

        /// Return the global controller registry
        static ControllerRegistry *the() {
            return gShared;
        }

        /// Adds a new controller to the registry.
        void addController(const std::shared_ptr<Controller> &controller);
        /// Returns a reference to all controllers.
        const auto &get() const {
            return this->controllers;
        }

    private:
        ControllerRegistry() = default;

    private:
        static ControllerRegistry *gShared;

        std::vector<std::shared_ptr<Controller>> controllers;
};

#endif
