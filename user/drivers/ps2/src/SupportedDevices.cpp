#include "PortDetector.h"

#include "device/Keyboard.h"

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
    // standard PS/2 mouse
    PortDetector::IdentifyDescriptor{
        1, {std::byte{0x03}}, "Generic PS/2 mouse"sv, [](auto controller, auto port) {
            return nullptr;
        }
    },
    // MF2 keyboard
    PortDetector::IdentifyDescriptor{
        2, {std::byte{0xAB}, std::byte{0x83}}, "MF2 keyboard"sv, [](auto controller, auto port) {
            return std::make_shared<Keyboard>(controller, port);
        }
    },
};
