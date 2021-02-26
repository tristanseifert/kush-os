#ifndef DYLDO_ELF_ELFREADER_H
#define DYLDO_ELF_ELFREADER_H

#include "Linker.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <utility>
#include <span>
#include <list>

#include <errno.h>
#include <sys/bitflags.hpp>
#include <sys/elf.h>
#include <sys/queue.h>

namespace dyldo {
struct Library;

/**
 * Protection level for a segment
 */
ENUM_FLAGS_EX(SegmentProtection, uint8_t);
enum class SegmentProtection: uint8_t {
    None                    = 0,
    Read                    = (1 << 0),
    Write                   = (1 << 1),
    Execute                 = (1 << 2),
};

/**
 * Base class for a ELF reader
 */
class ElfReader {
    public:
        /// Creates an ELF reader for a file that's already been opened.
        ElfReader(FILE * _Nonnull fp);
        /// Creates an ELF reader for the given file; if it doesn't exist, we abort.
        ElfReader(const char * _Nonnull path);
        virtual ~ElfReader();

        /// Gets info about all of the dependent libraries
        const auto &getDeps() {
            return this->deps;
        }

        /// Applies the given relocations.
        virtual void processRelocs(const std::span<Elf32_Rel> &rels) = 0;
        /// Gets the dynamic (data) relocation entries
        bool getDynRels(std::span<Elf32_Rel> &outRels);
        /// Gets the jump table (PLT) relocations
        bool getPltRels(std::span<Elf32_Rel> &outRels);

        /// Protects all loaded segments.
        void applyProtection();

    protected:
        /// Loads a segment described by a program header into memory.
        void loadSegment(const Elf32_Phdr &phdr, const uintptr_t base = 0);

        /// Reads the given number of bytes from the file at the specified offset.
        void read(const size_t nBytes, void * _Nonnull out, const uintptr_t offset);
        /// Parses the dynamic linker information.
        void parseDynamicInfo();

        /// Processes relocations.
        void patchRelocs(const std::span<Elf32_Rel> &rels, const uintptr_t base);

        /// Reads a string out of the string table at the given index.
        virtual const char * _Nullable readStrtab(const size_t i);

        /**
         * Converts a file view virtual address (read from some header in the ELF) to an actual
         * virtual address. This allows libraries to arbitrarily rebase themselves.
         */
        virtual uintptr_t rebaseVmAddr(const uintptr_t in) const {
            return in;
        }

        /// Processes a relocation indicating a copy from a shared object's data.
        void relocCopyFromShlib(const Elf32_Rel &rel, const SymbolMap::Symbol * _Nonnull sym,
                const uintptr_t base = 0);

    private:
        /// Determines the size of the file
        void getFilesize();
        /// Validates the ELF header.
        void validateHeader();
        /// Reads dependencies out of the dynamic linker info.
        void readDeps();

    protected:
        /// Contains the name of a library this object depends on.
        struct DependentLibrary {
            /// soname of the dependency
            char * _Nonnull name;

            DependentLibrary(const char * _Nonnull _name) {
                name = strdup(_name);
                if(!name) Linker::Abort("out of memory");
            }

            ~DependentLibrary() {
                // the name was created using strdup() out of the strtab
                free(reinterpret_cast<void *>(this->name));
            }
        };

        /**
         * Information on a loaded segment.
         */
        struct Segment {
            /// Offset into the ELF at which data for the segment begins
            uintptr_t offset = 0;
            /// Length of valid data
            size_t length = 0;

            /// Virtual memory base
            uintptr_t vmStart = 0;
            /// Ending virtual memory address
            uintptr_t vmEnd = 0;
            /// corresponding VM handle
            uintptr_t vmRegion = 0;

            /// desired protection level for the memory this segment represents
            SegmentProtection protection = SegmentProtection::None;
            /// Whether the VM protections have been restricted
            bool vmPermissionsRestricted = false;
        };

    protected:
        /// whether we're responsible for closing the file on dealloc
        bool ownsFile = false;
        /// file that we read from
        FILE *_Nonnull file;
        /// size of the file
        size_t fileSize = 0;

        /// string table
        std::span<char> strtab;
        /// symbol table
        std::span<Elf32_Sym> symtab;

        /// file offset to get to section headers
        uintptr_t shdrOff = 0;
        /// file offset to get to program headers
        uintptr_t phdrOff = 0;
        /// number of program headers
        size_t phdrNum = 0;
        /// number of section headers
        size_t shdrNum = 0;

        /// dynamic linker info
        std::span<Elf32_Dyn> dynInfo;

        /// list head of dependent libraries
        std::list<DependentLibrary> deps;
        /// segments we loaded from the file
        std::list<Segment> segments;

    private:
        static bool gLogSegments;
};
}

#endif
