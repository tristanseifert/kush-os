#ifndef DYLIB_LIBRARY_H
#define DYLIB_LIBRARY_H

#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <utility>

#include <sys/elf.h>

namespace dylib {
/**
 * Represents a loaded dynamic library.
 *
 * XXX: This is currently ONLY supporting 32-bit ELF. When we port to a 64-bit paltform, this needs
 * to be addressed, probably the same way the loader does it.
 */
class Library {
    public:
        /// Loads a library from an absolute path.
        static std::shared_ptr<Library> loadFile(const std::string &path);

    public:
        /// Gets the soname of the library
        const std::string &getSoname() const {
            return this->soname;
        }
        /// Returns the virtual memory requirements of the library
        const size_t getVmRequirements() const {
            size_t end = 0;
            for(const auto &segment : this->segments) {
                if(segment.vmEnd > end) {
                    end = segment.vmEnd;
                }
            }
            // since we want a length rather than last byte address, add 1.
            return end+1;
        }

        /// Creates a library that's backed by a file object.
        Library(FILE *);
        ~Library();

    private:
        /// Represents a pair of file VM address to length
        using AddrRange = std::pair<uintptr_t, size_t>;

        using DynMap = std::unordered_multimap<Elf32_Sword, Elf32_Word>;

        /**
         * Defines information on a segment of the library; this has a base address (relative to
         * where the library is loaded) and a length, at a minimum. Part (or all) of a region can
         * be backed by file contents.
         *
         * Lastly, the desired protection flags (applied before jumping to user code) are specified
         * for the segment.
         *
         * Note that the base and length values are not page aligned, but the vmStart/vmEnd values
         * are guaranteed to be. vmEnd refers to the last byte in the VM range that's allocated.
         * When copying data in, be sure to set the destination to vmStart plus the offset into the
         * page of the base address.
         */
        struct Segment {
            /// offset from library base
            uintptr_t base;
            /// total length of the region, in bytes
            size_t length;

            /// number of bytes to copy out of the file, if any
            size_t fileCopyBytes;
            /// offset into the file at which the data should be copied from
            uintptr_t fileOff;

            /// starting and ending address of the VM region
            uintptr_t vmStart, vmEnd;

            /**
             * Test if the two segments overlap.
             */
            const bool overlaps(Segment &in) const {
                auto x1 = this->base;
                auto x2 = x1 + this->length;
                auto y1 = in.base;
                auto y2 = y1 + in.length;

                return (x1 >= y1 && x1 <= y2) || (x2 >= y1 && x2 <= y2) ||
                       (y1 >= x1 && y1 <= x2) || (y2 >= x1 && y2 <= x2);
            }
        };

    private:
        bool validateHeader();
        bool readSegments();
        bool processSegment(const Elf32_Phdr &);
        bool processSegmentLoad(const Elf32_Phdr &);
        bool readDynInfo();
        bool readDynMandatory(const DynMap &);

        std::optional<std::string> readStrtabSlow(const uintptr_t, const size_t = 150);

    private:
        /// File stream we're reading from, if the library is currently open
        FILE *file = nullptr;

        /// file offset to segment headers
        uintptr_t phdrOff = 0;
        /// number of segment headers
        uintptr_t phdrNum = 0;

        /// file offset to the dynamic region
        uintptr_t dynOff = 0;
        /// length of the dynamic region
        uintptr_t dynLen = 0;

        /// extents of the string table
        AddrRange strtabExtents;
        /// base offset of the symbol table
        uintptr_t symtabOff = 0;
        /// size of a symbol entry
        uintptr_t symtabEntSz = 0;

        /// Library link name as extracted from its dynamic section
        std::string soname;
        /// all VM regions of the library
        std::vector<Segment> segments;

        /// install names of all dependent libraries
        std::vector<std::string> depNames;
};
}

#endif
