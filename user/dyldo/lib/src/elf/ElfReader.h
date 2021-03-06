#ifndef DYLDO_ELF_ELFREADER_H
#define DYLDO_ELF_ELFREADER_H

#include "Linker.h"
#include "struct/PaddedArray.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <utility>
#include <span>
#include <string_view>
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
    /// Dynamic link server port name
    constexpr static const std::string_view kDyldosrvPortName{"me.blraaz.rpc.dyldosrv"};
    /// Maximum receive message size
    constexpr static const size_t kMaxMsgLen{2048};

    public:
        /// Creates an ELF reader for a file that's already been opened.
        ElfReader(FILE * _Nonnull fp, const char * _Nonnull path);
        /// Creates an ELF reader for the given file; if it doesn't exist, we abort.
        ElfReader(const char * _Nonnull path);
        virtual ~ElfReader();

        /// Gets info about all of the dependent libraries
        const auto &getDeps() {
            return this->deps;
        }

        /// Applies the given relocations.
        virtual void processRelocs(const PaddedArray<Elf_Rel> &rels) = 0;
        /// Gets the dynamic (data) relocation entries
        bool getDynRels(PaddedArray<Elf_Rel> &outRels);
        /// Gets the jump table (PLT) relocations
        bool getPltRels(PaddedArray<Elf_Rel> &outRels);

        /// Gets an in-memory copy of the program headers.
        virtual bool getVmPhdrs(std::span<Elf_Phdr> &outPhdrs) {
            outPhdrs = this->phdrs;
            return !(this->phdrs.empty());
        }

        /// Protects all loaded segments.
        void applyProtection();

    protected:
        /// Loads a segment described by a program header into memory.
        void loadSegment(const Elf_Phdr &phdr, const uintptr_t base = 0);

        /// Reads the given number of bytes from the file at the specified offset.
        void read(const size_t nBytes, void * _Nonnull out, const uintptr_t offset);
        /// Parses the dynamic linker information.
        void parseDynamicInfo();

        /// Processes relocations.
        void patchRelocs(const PaddedArray<Elf_Rel> &rels, const uintptr_t base);

#if defined(__i386__) || defined(__amd64__)
        void patchRelocsi386(const PaddedArray<Elf_Rel> &rels, const uintptr_t base);
#endif
#if defined(__amd64__)
        void patchRelocsAmd64(const PaddedArray<Elf_Rela> &rels, const uintptr_t base);
#endif


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
        void relocCopyFromShlib(const Elf_Rel &rel, const SymbolMap::Symbol * _Nonnull sym,
                const uintptr_t base = 0);
        /// Processes a relocation indicating a copy from a shared object's data.
        void relocCopyFromShlib(const Elf_Rela &rel, const SymbolMap::Symbol * _Nonnull sym,
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
            uintptr_t offset{0};
            /// Length of valid data
            size_t length{0};

            /// Virtual memory base
            uintptr_t vmStart{0};
            /// Ending virtual memory address
            uintptr_t vmEnd{0};
            /// corresponding VM handle
            uintptr_t vmRegion{0};

            /// desired protection level for the memory this segment represents
            SegmentProtection protection{SegmentProtection::None};
            /// Whether the VM protections have been restricted
            bool vmPermissionsRestricted{false};
            /// When set, the segment is shared
            bool shared{false};
        };

    private:
        /// Checks if the dynamic link server is available.
        bool hasDyldosrv();
        /// Loads a shareable segment
        void loadSegmentShared(const Elf_Phdr &phdr, const uintptr_t base, Segment &seg);

    protected:
        /// ELF class
        uint8_t elfClass = 0;
        /// ELF machine type
        Elf_Half elfMachine = 0;

        /// whether we're responsible for closing the file on dealloc
        bool ownsFile = false;
        /// file that we read from
        FILE *_Nonnull file;
        /// size of the file
        size_t fileSize = 0;

        /// string table
        std::span<char> strtab;
        /// symbol table
        std::span<Elf_Sym> symtab;

        /// file offset to get to section headers
        uintptr_t shdrOff = 0;
        /// file offset to get to program headers
        uintptr_t phdrOff = 0;
        /// number of program headers
        size_t phdrNum = 0;
        /// number of section headers
        size_t shdrNum = 0;

        /// dynamic linker info
        std::span<Elf_Dyn> dynInfo;
        /// program headers
        std::span<Elf_Phdr> phdrs;

        /// list head of dependent libraries
        std::list<DependentLibrary> deps;
        /// segments we loaded from the file
        std::list<Segment> segments;

        // copy of the path this file was read from
        char * _Nullable path{nullptr};

    private:
        /// DyldoServer remote request port
        static uintptr_t gRpcServerPort;
        /// Port used to receive replies to RPC requests
        static uintptr_t gRpcReplyPort;
        /// Receive buffer for RPC replies
        static void * _Nullable gRpcReceiveBuf;

        static bool gLogSegments;
};
}

#endif
