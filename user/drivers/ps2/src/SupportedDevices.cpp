#include "PortDetector.h"

#include "device/Keyboard.h"
#include "device/GenericMouse.h"

#include <memory>

using namespace std::literals;

/**
 * Define the list of supported devices (based on their identification descriptors) that have
 * drivers built in.
 *
 * Note that this list is checked sequentially; the identification bytes are only compared if the
 * length is an exact match, and the first record that matches will be selected.
 */
const std::array<PortDetector::IdentifyDescriptor, PortDetector::kNumIdentifyDescriptors>
    PortDetector::gIdDescriptors{
    // plain PS/2 mouse
    PortDetector::IdentifyDescriptor{
        1, {std::byte{0x00}}, "Generic PS/2 mouse"sv, [](auto controller, auto port) {
            return std::make_shared<GenericMouse>(controller, port, true);
        }
    },
    // PS/2 mouse with wheel
    /*PortDetector::IdentifyDescriptor{
        1, {std::byte{0x03}}, "Generic PS/2 mouse with scroll wheel"sv, [](auto controller, auto port) {
            return std::make_shared<ThreeAxisMouse>(controller, port);
        }
    },*/
    // MF2 keyboard
    PortDetector::IdentifyDescriptor{
        2, {std::byte{0xAB}, std::byte{0x83}}, "MF2 keyboard"sv, [](auto controller, auto port) {
            return std::make_shared<Keyboard>(controller, port);
        }
    },
};
