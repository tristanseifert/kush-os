#ifndef DYLIB_LIBRARY_H
#define DYLIB_LIBRARY_H

#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>

#include <sys/bitflags.hpp>
#include <sys/elf.h>

#include "log.h"

namespace dylib {
/**
 * Flags defining a symbol's binding type (e.g. local/global/weak global) and its
 * visibility (e.g. default, internal, hidden, exported, etc.)
 *
 * Also defined is the object's type.
 */
ENUM_FLAGS_EX(SymbolFlags, uint16_t);
enum class SymbolFlags: uint16_t {
    None                                = 0,

    /// Locally bound symbol
    BindLocal                           = 1,
    /// Global symbol
    BindGlobal                          = 2,
    /// Weak global symbol
    BindWeakGlobal                      = 3,
    /// Mask for binding type
    BindMask                            = 0x000F,

    /// Unspecified object type
    TypeUnspecified                     = (0 << 4),
    /// Data (object)
    TypeData                            = (1 << 4),
    /// Function (code)
    TypeFunction                        = (2 << 4),
    /// Mask for symbol type
    TypeMask                            = 0x00F0,

    /// Default symbol visibility
    VisibilityDefault                   = (0 << 8),
    /// Mask for symbol visibility
    VisibilityMask                      = 0x0F00,

    /// When set, the symbol has been resolved.
    ResolvedFlag                        = 0x8000,
};

ENUM_FLAGS_EX(SegmentProtection, uint8_t);
enum class SegmentProtection: uint8_t {
    None                    = 0,
    Read                    = (1 << 0),
    Write                   = (1 << 1),
    Execute                 = (1 << 2),
};

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
        bool allocateProgbitsVm(const uintptr_t vmBase = 0);

        void closeFile();

        /// Quickly test if the given symbol is exported.
        bool exportsSymbol(const std::string &name) const;
        /// Gets a symbol's address and length.
        void getSymbolInfo(const std::string &name, std::pair<uintptr_t, size_t> &outInfo);
        /// Resolves external symbols against the list of libraries provided.
        bool resolveImports(const std::vector<std::pair<uintptr_t, std::shared_ptr<Library>>> &);
        /// Performs the relocations required for the library against the provided libraries.
        bool relocate(const std::vector<std::pair<uintptr_t, std::shared_ptr<Library>>> &);

        /// map all segments that are not writable as-is
        void mapShareable(const uintptr_t base, const uintptr_t taskHandle);
        // create copies of all writable segments
        void mapData(const uintptr_t base, const uintptr_t taskHandle);

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
        /// Do we have any more relocations?
        const bool hasUnresolvedRelos() const {
            return this->moreRelos;
        }

        /// Creates a library that's backed by a file object.
        Library(FILE *);
        ~Library();

    private:
        /// Represents a pair of file VM address to length
        using AddrRange = std::pair<uintptr_t, size_t>;

        using DynMap = std::unordered_multimap<Elf32_Sword, Elf32_Word>;

        /**
         * Provides information extracted from the ELF section headers. Symbol resolution requires
         * us to be able to look up sections, so we store their address information in these
         * structs.
         */
        struct Section {
            /**
             * Different types of sections as loaded from the ELF file.
             */
            enum class Type: uint8_t {
                None                    = 0,
                /// Pages backed by program data
                Data,
                /// Pages backed by anonymous memory
                AnonData,
                /// Symbol table
                Symtab,
                /// Dynamic symbol table
                DynamicSymtab,
                /// Dynamic linker information
                DynamicInfo,
                /// String table
                Strtab,
                /// Relocation information
                Relocation,

                /// Pre-initializer function
                PreInitArray,
                /// Initializers
                InitArray,
                /// Destructors
                FiniArray,

                /// Symbol hash table
                SymtabHash,
                /// GNU extension to symbol hash table
                SymtabHashGnu,
            };

            /// Virtual address (if loaded)
            uintptr_t addr = 0;
            /// Size of the section
            size_t length = 0;

            /// Section type
            Type type = Type::None;

            Section(const Elf32_Shdr &sec) {
                this->addr = sec.sh_addr;
                this->length = sec.sh_size;

                switch(sec.sh_type) {
                    case SHT_PROGBITS:
                        this->type = Type::Data;
                        break;
                    case SHT_NOBITS:
                        this->type = Type::AnonData;
                        break;
                    case SHT_REL:
                        this->type = Type::Relocation;
                        break;
                    case SHT_DYNAMIC:
                        this->type = Type::DynamicInfo;
                        break;
                    case SHT_SYMTAB:
                        this->type = Type::Symtab;
                        break;
                    case SHT_STRTAB:
                        this->type = Type::Strtab;
                        break;
                    case SHT_DYNSYM:
                        this->type = Type::DynamicSymtab;
                        break;
                    case SHT_PREINIT_ARRAY:
                        this->type = Type::PreInitArray;
                        break;
                    case SHT_INIT_ARRAY:
                        this->type = Type::InitArray;
                        break;
                    case SHT_FINI_ARRAY:
                        this->type = Type::FiniArray;
                        break;
                    case SHT_HASH:
                        this->type = Type::SymtabHash;
                        break;
                    case SHT_GNU_HASH:
                        this->type = Type::SymtabHashGnu;
                        break;

                    default:
                        L("Unknown section type {:08x}", sec.sh_type);
                        abort();
                        break;
                }
            }
            Section() = default;
        };

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

            SegmentProtection protection = SegmentProtection::None;

            /**
             * If nonzero, the handle to a virtual memory region that contains the part of the
             * segment that contains data loaded from the file. This can be all, or a subset of the
             * actual length of the segment.
             *
             * It's likely the `fileCopyBytes` value rounded up to the nearest page size.
             */
            uintptr_t vmRegion;
            /// Whether the VM protections have been restricted
            bool vmPermissionsRestricted = false;
            /// Whether there is data to be copied from the file
            bool progbits = false;

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

        /**
         * Information on a global symbol in the library. This can be either a symbol we export, or
         * a symbol that's imported from another dynamic library.
         */
        struct Symbol {
            /// name of the symbol
            std::string name;
            /// Symbol value and size
            std::pair<uintptr_t, size_t> data;
            /// flags
            SymbolFlags flags = SymbolFlags::None;
            /// section index this symbol occurs in (or 0 = none, 0xFFFF = abs)
            uint16_t sectionIdx = 0;

            /// Create a new symbol with the given name.
            Symbol(const std::string &_name) : name(_name) {}
            Symbol() = default;

            /// Gets the symbol's value
            const uintptr_t getValue() const {
                return this->data.first;
            }
            /// Gets the symbol's size
            const size_t getSize() const {
                return this->data.second;
            }
        };

    private:
        bool validateHeader();
        bool readSegments();
        bool processSegment(const Elf32_Phdr &);
        bool processSegmentLoad(const Elf32_Phdr &);
        bool readSectionHeaders();
        bool readDynInfo();
        bool readDynMandatory(const DynMap &);
        bool readDynSyms();
        bool parseSymtab(const std::span<char> &, const std::span<Elf32_Sym> &);
        bool processRelocs(const std::span<Elf32_Rel> &, const uintptr_t,
                const std::vector<std::pair<uintptr_t, std::shared_ptr<Library>>> &);
        bool processPltRelocs(const std::span<Elf32_Rel> &, const uintptr_t);

        std::optional<std::string> readStrtab(const uintptr_t i);
        std::optional<std::string> readStrtabSlow(const uintptr_t, const size_t = 150);

        static uintptr_t resolveSymbolVmAddr(const std::string &,
                const std::vector<std::pair<uintptr_t, std::shared_ptr<Library>>> &);

    private:
        /// File stream we're reading from, if the library is currently open
        FILE *file = nullptr;

        /// file offset to section headers
        uintptr_t shdrOff = 0;
        /// number of section headers
        size_t shdrNum = 0;

        /// file offset to segment headers
        uintptr_t phdrOff = 0;
        /// number of segment headers
        size_t phdrNum = 0;

        /// file offset to the dynamic region
        uintptr_t dynOff = 0;
        /// length of the dynamic region
        size_t dynLen = 0;

        /// extents of the string table
        AddrRange strtabExtents;
        /// base offset of the symbol table
        uintptr_t symtabOff = 0;
        /// size of a symbol entry
        size_t symtabEntSz = 0;

        /// size of the dynsym region
        size_t dynsymLen = 0;

        /**
         * Indicates whether there are any relocations that need to be applied. This flag is
         * cleared after relocations have been applied and ALL symbols were resolved successfully.
         */
        bool moreRelos = true;

        /// Library link name as extracted from its dynamic section
        std::string soname;
        /// loaded sections in the library
        std::vector<Section> sections;
        /// all VM regions of the library
        std::vector<Segment> segments;

        /// install names of all dependent libraries
        std::vector<std::string> depNames;

        /// global symbols in the library
        std::vector<Symbol> syms;
        //std::unordered_map<std::string, Symbol> syms;

        /// temporary string table: discarded after all relocations are processed
        std::vector<char> strtabTemp;
};
}

#endif
