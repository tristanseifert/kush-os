#include "PortDetector.h"
#include "Ps2Controller.h"
#include "Log.h"

/**
 * Initializes the port detector.
 */
PortDetector::PortDetector(Ps2Controller *_controller, const Ps2Controller::Port _port) :
    controller(_controller), port(_port) {

}
