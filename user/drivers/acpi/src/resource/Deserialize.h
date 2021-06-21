#ifndef RESOURCE_DESERIALIZE_H
#define RESOURCE_DESERIALIZE_H

#include <acpi.h>

struct mpack_node_t;

namespace acpi {
/// Deserialize an ACPI resource from the given map node.
void deserialize(mpack_node_t &, ACPI_RESOURCE &out);
}

#endif
