#ifndef PORTDETECTOR_H
#define PORTDETECTOR_H

#include "Ps2Controller.h"

/**
 * Implements the detection state machine that determines what device is connected to a particular
 * port of the PS/2 controller.
 */
class PortDetector {
    public:
        PortDetector(Ps2Controller * _Nonnull controller, const Ps2Controller::Port port);

    private:
        /// controller that instantiated us
        Ps2Controller * _Nonnull controller;
        // port on the controller
        Ps2Controller::Port port;
};

#endif
