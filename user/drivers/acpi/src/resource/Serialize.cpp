#include "Serialize.h"

#include <acpi.h>
#include <mpack/mpack.h>

using namespace acpi;

/**
 * Serializes ACPI resources to msgpack structures.
 *
 * All maps use integer-based indexing. Index 1 is reserved to hold the type of the structure,
 * which uses the existing ACPI object type values.
 */

/**
 * Serialize an ACPI interrupt resource.
 */
void acpi::serialize(mpack_writer_t *writer, const ACPI_RESOURCE_IRQ &irq) {
    mpack_start_map(writer, 6);

    mpack_write_u8(writer, 0);
    mpack_write_u8(writer, ACPI_RESOURCE_TYPE_IRQ);

    mpack_write_u8(writer, 1); // edge sensitive
    mpack_write_bool(writer, irq.Triggering == ACPI_EDGE_SENSITIVE);
    mpack_write_u8(writer, 2); // polarity
    mpack_write_u8(writer, irq.Polarity);
    mpack_write_u8(writer, 3); // is exclusive?
    mpack_write_bool(writer, irq.Shareable == ACPI_EXCLUSIVE);
    mpack_write_u8(writer, 4); // can wake system?
    mpack_write_bool(writer, irq.WakeCapable == ACPI_WAKE_CAPABLE);
    mpack_write_u8(writer, 5); // irq number
    mpack_write_u8(writer, irq.Interrupts[0]);

    mpack_finish_map(writer);
}

/**
 * Serialize an ACPI IO resource.
 */
void acpi::serialize(mpack_writer_t *writer, const ACPI_RESOURCE_IO &io) {
    mpack_start_map(writer, 6);

    mpack_write_u8(writer, 0);
    mpack_write_u8(writer, ACPI_RESOURCE_TYPE_IO);

    mpack_write_u8(writer, 1); // supports 16-bit address decoding
    mpack_write_bool(writer, io.IoDecode == ACPI_DECODE_16);

    mpack_write_u8(writer, 2); // alignment requirement
    mpack_write_u8(writer, io.Alignment);
    mpack_write_u8(writer, 3); // length of addresses (width?)
    mpack_write_u8(writer, io.AddressLength);

    mpack_write_u8(writer, 4); // minimum address
    mpack_write_u16(writer, io.Minimum);
    mpack_write_u8(writer, 5); // maximum address
    mpack_write_u16(writer, io.Maximum);

    mpack_finish_map(writer);
}

