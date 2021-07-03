#include "Library.h"
#include "Log.h"
#include "PacketTypes.h"
#include "hashmap.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>

/**
 * Initializes the library object.
 */
Library::Library(char *_path) : path(_path) {
    this->segments = hashmap_new(sizeof(Segment), 4, 'DYLD', 'SEGS', HashSegment, CompareSegment,
            nullptr);
}

/**
 * Cleans up library resources.
 */
Library::~Library() {
    free(this->path);
    hashmap_free(this->segments);
}



/**
 * Calculates the hash of the given segment; this is over the "address" field.
 */
uint64_t Library::HashSegment(const void *item, uint64_t s0, uint64_t s1) {
    auto e = reinterpret_cast<const Segment *>(item);
    return hashmap_sip(&e->a, sizeof(e->a), s0, s1);
}

/**
 * Compares two segments by doing a memory compare against the "address" region.
 */
int Library::CompareSegment(const void *a, const void *b, void *) {
    auto e1 = reinterpret_cast<const Segment *>(a);
    auto e2 = reinterpret_cast<const Segment *>(b);

    //return memcmp(&e1->a, &e2->a, sizeof(e1->a));
    return e1->a.virt < e2->a.virt;
}



/**
 * Adds a new segment to the library.
 */
void Library::addSegment(const DyldosrvMapSegmentRequest &req, const uintptr_t vmRegion) {
    // build the entry
    Segment seg(req.phdr);
    seg.region = vmRegion;

    // insert
    auto ret = hashmap_set(this->segments, &seg);
    if(!ret && hashmap_oom(this->segments)) {
        Abort("%s failed: %d", "hashmap_set", ENOMEM);
    }
}

/**
 * Looks up a segment that covers the given program header, and returns the virtual memory object
 * handle that represents it.
 *
 * @return VM handle or 0 if not found
 */
uintptr_t Library::regionFor(const Elf_Phdr &phdr) {
    Segment seg(phdr);
    auto ret = hashmap_get(this->segments, &seg);
    if(!ret) return 0;

    auto found = reinterpret_cast<const Segment *>(ret);
    return found->region;
}

