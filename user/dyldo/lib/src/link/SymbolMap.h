#ifndef DYLDO_LINK_SYMBOLMAP_H
#define DYLDO_LINK_SYMBOLMAP_H

#include <sys/bitflags.hpp>
#include <sys/elf.h>

#include "struct/hashmap.h"

namespace dyldo {
struct Library;

ENUM_FLAGS_EX(SymbolFlags, uint16_t);
/**
 * Flags defining a symbol's type, visibility, and object type
 */
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
    /// Thread-local storage reference
    TypeThreadLocal                     = (3 << 4),
    /// Mask for symbol type
    TypeMask                            = 0x00F0,

    /// Default symbol visibility
    VisibilityDefault                   = (0 << 8),
    /// Mask for symbol visibility
    VisibilityMask                      = 0x0F00,
};

/**
 * Registry mapping symbol names (from a library's string allocator) to their values.
 */
class SymbolMap {
    /**
     * Initial size of the symbol hashmap.
     *
     * This is set to allow roughly containing most of the commonly used functions from the C and
     * system libraries.
     */
    constexpr static const size_t kMapInitialSize = 128;
    /**
     * Initial size of the overrides hashmap.
     *
     * This is sized to allow enough overrides to get all functions in the C library to work.
     */
    constexpr static const size_t kOverrideMapInitialSize = 16;

    public:
        struct Symbol {
            /// pointer to the symbol's name
            const char * _Nonnull name;
            /// library this symbol is exported from
            Library * _Nonnull library;

            /// symbol address (absolute)
            uintptr_t address;
            /// number of bytes for the symbol
            size_t length = 0;

            // flags defining this symbol's mapping
            SymbolFlags flags = SymbolFlags::None;
        };

    public:
        SymbolMap();

        /// Adds a new symbol.
        void add(const char * _Nonnull name, const Elf_Sym &sym, Library * _Nonnull library);
        /// Registers a symbol override.
        void addOverride(const Symbol * _Nonnull inSym, const uintptr_t newAddr);
        /// Adds a new linker exported symbol in the form of a function.
        void addLinkerExport(const char * _Nonnull name, const void(* _Nonnull function)(void)) {
            this->addLinkerExport(name, reinterpret_cast<void *>(function), 0);
        }
        /// Adds a new linker exported symbol as a blob of data.
        void addLinkerExport(const char * _Nonnull name, const void * _Nonnull data,
                const size_t length);
        /// Gets symbol information, if found.
        Symbol const * _Nullable get(const char * _Nonnull name, Library * _Nullable searchIn = nullptr);

        /// Gets the number of registered symbols
        const size_t getNumSymbols() const {
            return hashmap_num_entries(&this->map);
        }

    private:
        /// whether overrides are logged
        static bool gLogOverrides;

    private:
        /**
         * Mapping of symbol name -> symbol info, for global symbols exported by all loaded
         * dynamic objects.
         */
        struct hashmap_s map;

        /**
         * Mapping of symbol name -> symbol info, for symbol overrides. These are added by the
         * linker during the linking process, similar to weak symbols.
         *
         * This is used so that when we copy data out of shared libraries and into the executable
         * data segment, later shared libraries see the "correct" address.
         */
        struct hashmap_s overridesMap;
};
}

#endif
