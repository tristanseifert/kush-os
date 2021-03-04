#ifndef PS2CONTROLLER_H
#define PS2CONTROLLER_H

#include <cstddef>
#include <cstdint>
#include <span>

struct mpack_node_t;

/**
 * Encapsulates the 8042 PS/2 controller. This supports the most commonly found dual-port variant
 * of the controller.
 */
class Ps2Controller {
    public:
        Ps2Controller(const std::span<std::byte> &aux);

    private:
        void decodeResources(mpack_node_t &node, const bool isKbd);

    private:
        /// whether this is a dual-port controller with mouse support
        bool hasMouse = false;

        /// IRQ for the keyboard port
        uintptr_t kbdIrq = 0;
        /// IRQ for the mouse port
        uintptr_t mouseIrq = 0;
};

#endif
