#ifndef DYLDO_H
#define DYLDO_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include <sys/elf.h>

#include "LaunchInfo.h"
#include "link/SymbolMap.h"
#include "struct/hashmap.h"

namespace dyldo {
class ElfReader;
class ElfExecReader;
class ElfLibReader;
struct Library;
class SymbolMap;

/**
 * Global instance of the dynamic linker.
 *
 * This encapsulates the entire dynamic linking process, and keeps track of all libraries we've
 * loaded.
 */
class Linker {
    public:
        /// base address for shared libraries
        constexpr static const uintptr_t kSharedLibBase = 0xA0000000;
        /// desired alignment for shared libraries
        constexpr static const uintptr_t kLibAlignment = 0x100000;

        /// system search paths
        constexpr static const std::array<const char *, 3> kDefaultSearchPaths = {
            "/lib",
            "/usr/lib",
            "/usr/local/lib",
        };

    public:
        /// Initializes the shared linker.
        static void init(const char *_Nonnull execPath) {
            gShared = new Linker(execPath);
            if(!gShared) {
                Abort("out of memory");
            }
        }
        /// Returns the shared linker.
        static Linker *_Nonnull the() {
            return gShared;
        }

        /// Outputs a message if trace logging is enabled
        static void Trace(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
        /// Outputs an informational message
        static void Info(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
        /// Outputs an error message and exits the task.
        [[noreturn]] static void Abort(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));

    public:
        Linker(const char * _Nonnull path);

        /// Loads libraries required by the executable (and other libraries)
        void loadLibs();
        /// Performs relocations and other fixups on loaded data.
        void doFixups();
        /// Releases caches and other buffers we don't need during runtime
        void cleanUp();
        /// Runs initializers of loaded libraries.
        void runInit();
        /// Transfers control to the entry point of the loaded program.
        [[noreturn]] void jumpToEntry(const kush_task_launchinfo_t * _Nonnull);

        /// Resolves a symbol.
        const SymbolMap::Symbol* _Nullable resolveSymbol(const char *_Nonnull name,
                Library * _Nullable inLibrary = nullptr);
        /// Registers a symbol exported from a library
        void exportSymbol(const char * _Nonnull name, const Elf32_Sym &sym, Library *_Nonnull lib);
        /// Overrides a symbol's address.
        void overrideSymbol(const SymbolMap::Symbol * _Nonnull inSym, const uintptr_t newAddr);

    private:
        /// Load a shared library
        void loadSharedLib(const char * _Nonnull soname);
        /// Searches for a library with the given name in system paths and open it
        FILE * _Nullable openSharedLib(const char * _Nonnull soname, const char * _Nullable &outPath);

    private:
        /// shared linker instance
        static Linker * _Nullable gShared;
        /// are trace logs enabled?
        static bool gLogTraceEnabled;

        /// whether we log initializer/destructor invocations
        static bool gLogInitFini;

    private:
        /// ELF reader for the executable
        ElfExecReader * _Nullable exec = nullptr;

        /// base address to load the next shared library at
        uintptr_t soBase = kSharedLibBase;
        /// memory address holding program entry point
        uintptr_t entryAddr = 0;

        /**
         * Map of libraries loaded; we build this up during the loading process, and it can be
         * referred back to later using the dlsym-type calls.
         *
         * Additionally, all symbols in the global symbol table will map to a library object that's
         * pointed to by this map.
         */
        struct hashmap_s loaded;

        /**
         * Symbol registration map; all symbols from loaded dynamic libraries are stored in here
         * so we can look them up later, during relocations and during runtime.
         */
        SymbolMap * _Nonnull map;
};
}

#endif
