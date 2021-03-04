#include "Ps2Controller.h"
#include "Log.h"

#include <Deserialize.h>

#include <mpack/mpack.h>

/**
 * Initializes a PS/2 controller with the provided aux data structure. This should be a map, with
 * the "kbd" and "mouse" keys; each key may either be nil (if that port is not supported) or a
 * list of resources that are assigned to it.
 *
 * The kbd port lists the IO resources for the controller.
 */
Ps2Controller::Ps2Controller(const std::span<std::byte> &aux) {
    // set up a reader
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(aux.data()), aux.size());
    mpack_tree_parse(&tree);

    mpack_node_t root = mpack_tree_root(&tree);

    // have we kbd info?
    auto kbdNode = mpack_node_map_cstr(root, "kbd");
    if(mpack_node_type(kbdNode) != mpack_type_array) {
        Abort("no kbd info");
    }

    this->decodeResources(kbdNode, true);

    // get mouse info as well
    auto mouseNode = mpack_node_map_cstr(root, "mouse");
    if(mpack_node_type(mouseNode) == mpack_type_array) {
        this->decodeResources(mouseNode, false);
        this->hasMouse = true;
    } else {
        Warn("No mouse port for PS/2 controller");
    }

    // perform controller reset
    Trace("PS/2 controller: kbd IRQ %2u mouse IRQ %2u", this->kbdIrq, this->mouseIrq);
}

/**
 * Decodes the resources in the given array.
 */
void Ps2Controller::decodeResources(mpack_node_t &list, const bool isKbd) {
    ACPI_RESOURCE rsrc;

    const size_t nResources = mpack_node_array_length(list);

    // decode each resource
    for(size_t i = 0; i < nResources; i++) {
        auto child = mpack_node_array_at(list, i);
        acpi::deserialize(child, rsrc);

        switch(rsrc.Type) {
            case ACPI_RESOURCE_TYPE_IRQ: {
                const auto &irq = rsrc.Data.Irq;
                if(isKbd) {
                    this->kbdIrq = irq.Interrupts[0];
                } else {
                    this->mouseIrq = irq.Interrupts[0];
                }
                break;
            }

            case ACPI_RESOURCE_TYPE_IO: {
                const auto &io = rsrc.Data.Io;
                Trace("IO: %04x - %04x (align %u addr len %u decode %u)", io.Minimum, io.Maximum,
                        io.Alignment, io.AddressLength, io.IoDecode);
                break;
            }

            default:
                Abort("Unsupported ACPI resource type %u", rsrc.Type);
        }
    }
}
