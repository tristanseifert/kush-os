#pragma once

#include <cstddef>
#include <cstdint>

#include <sys/elf.h>

struct hashmap;
struct DyldosrvMapSegmentRequest;

/**
 * Represents a single shared library as identified by its full path.
 */
class Library {
    public:
        Library(char *path);
        ~Library();

        /// Gets the path this library was loaded from.
        constexpr const char *getPath() const {
            return this->path;
        }

        /// Adds information about a newly loaded shareable segment
        void addSegment(const DyldosrvMapSegmentRequest &req, const uintptr_t vmRegion);
        /// Gets information for a segment based on a program header
        uintptr_t regionFor(const Elf_Phdr &phdr);

    private:
        /**
         * Information structure for a segment
         */
        struct Segment {
            struct {
                /// File offset
                uintptr_t fileOff{0};
                /// Total size of segment (bytes)
                size_t length{0};
                /// Virtual address (relative to library base)
                uintptr_t virt{0};
            } a;

            /// VM region handle
            uintptr_t region{0};

            Segment() = default;
            Segment(const Elf_Phdr &p) : a({p.p_offset, p.p_memsz, p.p_vaddr}) {}
        };

        static uint64_t HashSegment(const void *_item, uint64_t s0, uint64_t s1);
        static int CompareSegment(const void *a, const void *b, void *udata);

    private:
        /// Path from which the library was loaded
        char *path{nullptr};

        /// map of all segments we've loaded
        hashmap *segments{nullptr};
};
