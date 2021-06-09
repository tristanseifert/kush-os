#include "PortDetector.h"
#include "Ps2Controller.h"
#include "Ps2IdentifyCommand.h"
#include "Log.h"

#include <algorithm>

/**
 * Initializes the port detector.
 */
PortDetector::PortDetector(Ps2Controller *_controller, const Ps2Controller::Port _port) :
    controller(_controller), port(_port) {
    // TODO: anything here?
}

/**
 * Reset the state of the port device detector machinery.
 *
 * This can be called in response to a manual reset (after the device has acknowledged the reset
 * command) or when we've timed out receiving data from the device, under the assumption that it
 * has been disconnected.
 */
void PortDetector::reset() {
    this->state = State::Idle;
}

/**
 * Handles a byte of detection data that has been sent.
 */
void PortDetector::handleRx(const std::byte data) {
    switch(this->state) {
        /**
         * When idle, we expect to receive a device's self test status code. This is sent when the
         * device powers up (is connected to the machine) or has just been reset via the reset
         * command.
         *
         * If the self test passes, we'll disable scanning on the device, then request that the
         * device returns its identification.
         */
        case State::Idle:
            if(data == kSelfTestPassReply) {
                auto cmd = Ps2Command::DisableUpdates([](auto p, auto cmd, auto ctx) {
                    auto detector = reinterpret_cast<PortDetector *>(ctx);

                    if(cmd->didCompleteSuccessfully()) {
                        detector->identifyDevice();
                    } else {
                        Warn("%s failed for device on %s port", "disabling updates",
                                p == Ps2Controller::Port::Primary ? "primary" : "secondary");
                        detector->deviceFailed();
                    }
                }, this);

                this->controller->submit(this->port, cmd);
            } else {
                Warn("Received BAT code $%02x from device %lu", data, (size_t) this->port);
                this->state = State::FaultyDevice;
            }
            break;

        /// should not receive a byte in this state
        case State::Identify:
            Abort("BUG: received byte $%02x for device on %s port in identify stage", data,
                    this->port == Ps2Controller::Port::Primary ? "primary" : "secondary");
            break;

        /**
         * The identification command has completed.
         */
        case State::IdentifyComplete:
            Abort("BUG: received byte $%02x for device on %s port in identify complete", data,
                    this->port == Ps2Controller::Port::Primary ? "primary" : "secondary");
            break;

        /**
         * Ignore any bytes received from faulty devices.
         */
        case State::FaultyDevice:
            Warn("Received byte $%02x from faulty device %lu", data, (size_t) this->port);
            break;
    }
}

/**
 * Mark the detection process as failed.
 */
void PortDetector::deviceFailed() {
    this->state = State::FaultyDevice;
    this->controller->detectionCompleted(this->port, false);
}

/**
 * Sends the "identify device" request.
 */
void PortDetector::identifyDevice() {
    this->state = State::Identify;

    // set up the command
    auto cmd = std::make_shared<Ps2IdentifyCommand>([](auto p, auto cmd, auto ctx) {
        auto detector = reinterpret_cast<PortDetector *>(ctx);

        if(cmd->didCompleteSuccessfully()) {
            detector->handleIdentify(std::span{cmd->replyBytes});
        } else {
            Warn("%s failed for device on %s port", "identify",
                    p == Ps2Controller::Port::Primary ? "primary" : "secondary");
            detector->deviceFailed();
        }
    }, this);

    // and submit it
    this->controller->submit(this->port, cmd);
}

/**
 * Process a device's identification. The provided buffer contains anywhere between zero to two
 * bytes of identification data, which is compared against our internal map of known PS/2 devices.
 */
void PortDetector::handleIdentify(const std::span<std::byte> &id) {
    // try to match it against a driver
    for(const auto &match : gIdDescriptors) {
        // skip if length isn't a match
        if(match.numIdentifyBytes != id.size()) continue;
        // compare the ID bytes (or match if both are empty)
        if(id.empty() ||
           std::equal(id.begin(), id.end(), match.identifyBytes.begin())) {
            // if equal, create the device
            Success("Device on %s port is '%s'",
                    this->port == Ps2Controller::Port::Primary ? "primary" : "secondary",
                    match.name);

            auto device = match.construct(this->controller, this->port);

            this->controller->setDevice(this->port, std::move(device));
            this->controller->detectionCompleted(this->port, true);
            return;
        }
    }

    // failed to match device
    Warn("Failed to identify device on %s port! (got %lu id bytes, first is $%02x)",
            this->port == Ps2Controller::Port::Primary ? "primary" : "secondary", id.size(),
            id[0]);
    this->controller->detectionCompleted(this->port, false);
}
