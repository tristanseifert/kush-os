#ifndef RESOURCE_SERIALIZE_H
#define RESOURCE_SERIALIZE_H

#include <acpi.h>

struct mpack_writer_t;

namespace acpi {
void serialize(mpack_writer_t * _Nonnull, const ACPI_RESOURCE_IRQ &);
void serialize(mpack_writer_t * _Nonnull, const ACPI_RESOURCE_IO &);
}

#endif
