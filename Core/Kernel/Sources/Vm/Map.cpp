#include "Vm/Map.h"
#include "Vm/Manager.h"
#include "Vm/MapEntry.h"

#include "Logging/Console.h"

#include <Intrinsics.h>
#include <Platform.h>

using namespace Kernel::Vm;

/**
 * Kernel virtual memory map
 *
 * The first VM object created is assigned to this variable. Any subsequently created maps which do
 * not explicitly specify a parent value will use this map as their parent, so that any shared
 * kernel data can be provided to them all.
 */
Map *Map::gKernelMap{nullptr};

/**
 * @brief Initialize a new map.
 *
 * @param parent A map to use as its parent; if `nullptr` is specified, the kernel default map is
 *        assumed to be the parent.
 */
Map::Map(Map *parent) : parent(parent ? parent->retain() : nullptr),
    pt(Platform::PageTable(this->parent ? &this->parent->pt : nullptr)) {
}

/**
 * @brief Activates this virtual memory map on the calling processor.
 *
 * This just thunks directly to the platform page table handler, which will invoke some sort of
 * processor-specific stuff to actually load the page tables.
 */
void Map::activate() {
    this->pt.activate();
}

/**
 * @brief Adds the given map entry to this map.
 *
 * @param base Base address for the mapping. The entire region of `[base, base+length]` must be
 *        available in the map.
 * @param entry The memory map entry to map. It will be retained.
 *
 * @return 0 on success or a negative error code.
 */
int Map::add(const uintptr_t base, MapEntry *entry) {
    if(!base || !entry) {
        // TODO: standardized error codes
        return -1;
    }

    // TODO: check for overlap

    // TODO: add it to a list
    entry = entry->retain();

    // notify the entry (so it may update the pagetable)
    entry->addedTo(base, *this, this->pt);

    return 0;
}
