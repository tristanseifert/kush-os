#include "Deserialize.h"

#include <stdexcept>

#include <acpi.h>
#include <mpack/mpack.h>

static void deserialize(mpack_node_t &, ACPI_RESOURCE_IRQ &);
static void deserialize(mpack_node_t &, ACPI_RESOURCE_IO &);

/**
 * Attempt to deserialize an ACPI resource.
 */
void acpi::deserialize(mpack_node_t &node, ACPI_RESOURCE &out) {
    // try to get the type
    auto typeNode = mpack_node_map_int(node, 0);
    if(mpack_node_type(typeNode) != mpack_type_uint) {
        throw std::runtime_error("invalid type field");
    }

    auto type = mpack_node_u8(typeNode);

    switch(type) {
        case ACPI_RESOURCE_TYPE_IRQ:
            out.Type = ACPI_RESOURCE_TYPE_IRQ;
            deserialize(node, out.Data.Irq);
            break;
        case ACPI_RESOURCE_TYPE_IO:
            out.Type = ACPI_RESOURCE_TYPE_IO;
            deserialize(node, out.Data.Io);
            break;

        default:
            throw std::runtime_error("unsupported ACPI resource type");
    }

    // XXX: technically wrong if caller didn't allocate the entire structure
    out.Length = sizeof(ACPI_RESOURCE);
}

/**
 * Deserializes an IRQ info structure.
 */
static void deserialize(mpack_node_t &node, ACPI_RESOURCE_IRQ &irq) {
    irq.Triggering = mpack_node_bool(mpack_node_map_int(node, 1)) ? 
        ACPI_EDGE_SENSITIVE : ACPI_LEVEL_SENSITIVE;

    irq.Polarity = mpack_node_u8(mpack_node_map_int(node, 2));

    irq.Shareable = mpack_node_bool(mpack_node_map_int(node, 3)) ? 
        ACPI_EXCLUSIVE : ACPI_SHARED;
    irq.WakeCapable = mpack_node_bool(mpack_node_map_int(node, 4)) ? 
        ACPI_WAKE_CAPABLE : ACPI_NOT_WAKE_CAPABLE;

    irq.InterruptCount = 1;
    irq.Interrupts[0] = mpack_node_u8(mpack_node_map_int(node, 5));
}

/**
 * Deserializes an IO port info structure.
 */
static void deserialize(mpack_node_t &node, ACPI_RESOURCE_IO &io) {
    // 16-bit address decoding supported
    if(mpack_node_bool(mpack_node_map_int(node, 1))) {
        io.IoDecode = ACPI_DECODE_16;
    } else {
        io.IoDecode = ACPI_DECODE_10;
    }

    io.Alignment = mpack_node_u8(mpack_node_map_int(node, 2));
    io.AddressLength = mpack_node_u8(mpack_node_map_int(node, 3));
    io.Minimum = mpack_node_u16(mpack_node_map_int(node, 4));
    io.Maximum = mpack_node_u16(mpack_node_map_int(node, 5));
}
