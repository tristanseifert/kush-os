#include "Vm/Map.h"
#include "Vm/Manager.h"
#include "Vm/MapEntry.h"

#include "Logging/Console.h"

#include <Intrinsics.h>
#include <Platform.h>

using namespace Kernel::Vm;

/**
 * @brief Initializes a virtual memory object.
 *
 * @param length Size of the virtual memory region, in bytes
 * @param mode Desired access mode for mapped pages
 */
MapEntry::MapEntry(const size_t length, const Mode mode) : length(length), accessMode(mode) {
    REQUIRE(!(length % Platform::PageTable::PageSize()),
            "length (%zu) is not a page size multiple (%zu)", length,
            Platform::PageTable::PageSize());

    // TODO: do stuff
}

