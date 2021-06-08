#ifndef PS2DEVICE_H
#define PS2DEVICE_H

#include "Ps2Controller.h"

#include <cstdint>

class Ps2Device {
    public:
        Ps2Device(Ps2Controller * _Nonnull _controller, const Ps2Port _port) :
            controller(_controller), port(_port) {};
        virtual ~Ps2Device() = default;

        /// A byte of data was received on the device's port not corresponding to a command.
        virtual void handleRx(const std::byte data) = 0;

    protected:
        /// Submits a command to the port this device is connected to
        inline void submit(Ps2Command &cmd) {
            this->controller->submit(this->port, cmd);
        }

    protected:
        /// controller that instantiated us
        Ps2Controller * _Nonnull controller;
        // port on the controller
        Ps2Port port;
};

#endif
