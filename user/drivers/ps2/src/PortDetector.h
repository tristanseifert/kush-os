#ifndef PORTDETECTOR_H
#define PORTDETECTOR_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>

#include "Ps2Controller.h"

class Ps2Device;

/**
 * Implements the detection state machine that determines what device is connected to a particular
 * port of the PS/2 controller.
 */
class PortDetector {
    /// Byte sent from device to indicate self test passed
    constexpr static const std::byte kSelfTestPassReply{0xAA};

    enum class State {
        /*
         * Port detector is idle; we wait for a device to be connected and send its self test
         * status code. We may also receive a dummy "command acknowledge" byte if this is the
         * result of a manual device reset, which is ignored.
         */
        Idle,

        /**
         * The device has passed self test, so interrogate it to determine what device type we are
         * dealing with.
         */
        Identify,

        /**
         * The identify command has completed, so interpret its response and figure out what type
         * of device is connected.
         */
        IdentifyComplete,

        /**
         * The port this detector is responsible for has a faulty device connected.
         */
        FaultyDevice,
    };

    /**
     * Descriptor for a device, based on its identification
     */
    struct IdentifyDescriptor {
        using DevicePtr = std::shared_ptr<Ps2Device>;
        using Constructor = std::function<DevicePtr(Ps2Controller * _Nonnull, const Ps2Port)>;

        /// number of bytes of identify data
        const uint8_t numIdentifyBytes;
        /// up to 2 bytes of identify data
        const std::array<std::byte, 2> identifyBytes;

        /// descriptive name of this type of device
        const std::string_view name;

        /// function to invoke to construct the device object
        const Constructor construct;
    };

    public:
        PortDetector(Ps2Controller * _Nonnull controller, const Ps2Port port);

        /// Reset the detection state machine
        void reset();
        /// Handles a received byte
        void handleRx(const std::byte data);

    private:
        /// Sends the "identify device" request.
        void identifyDevice();
        /// Indicates that some part of the detection process has failed.
        void deviceFailed();
        /// Determine what type of device is connected to the port, based on its identify reply.
        void handleIdentify(const std::span<std::byte> &id);

    private:
        /// controller that instantiated us
        Ps2Controller * _Nonnull controller;
        /// port on the controller
        Ps2Port port;

        /// internal state machine state
        State state = State::Idle;

        /// Number of identification descriptors for built in drivers
        constexpr static const size_t kNumIdentifyDescriptors = 2;
        /// Built in drivers
        static const std::array<IdentifyDescriptor, kNumIdentifyDescriptors> gIdDescriptors;
};

#endif
