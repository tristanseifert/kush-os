#ifndef DYLDO_H
#define DYLDO_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <list>
#include <span>
#include <string_view>

#include <sys/elf.h>

#include "LaunchInfo.h"
#include "link/SymbolMap.h"
#include "struct/hashmap.h"
#include "runtime/ThreadLocal.h"

namespace dyldo {
class DlInfo;
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
    friend class DlInfo;
    friend class ElfExecReader;
    friend class ThreadLocal;

    public:
#if defined(__i386__)
        /// base address for shared libraries
        constexpr static const uintptr_t kSharedLibBase = 0xA0000000;
        /// desired alignment for shared libraries
        constexpr static const uintptr_t kLibAlignment = 0x100000;
#elif defined(__amd64__)
        /// base address for shared libraries
        constexpr static const uintptr_t kSharedLibBase = 0xA0000000;
        /// desired alignment for shared libraries
        constexpr static const uintptr_t kLibAlignment = 0x200000;
#else
#error Please define shared library base address for current architecture
#endif

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
            gShared->secondInit();
        }
        /// Returns the shared linker.
        static Linker *_Nonnull the() {
            return gShared;
        }

#ifdef DYLDO_VERBOSE
        /// Outputs a message if trace logging is enabled
        static void Trace(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
#else
        static void Trace(const char * _Nonnull, ...) __attribute__ ((format (printf, 1, 2))) {};
#endif
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

        /// Prints all loaded libraries and their base addresses
        void printImageBases();

        /// Resolves a symbol.
        const SymbolMap::Symbol* _Nullable resolveSymbol(const char *_Nonnull name,
                Library * _Nullable inLibrary = nullptr);
        /// Registers a symbol exported from a library
        void exportSymbol(const char * _Nonnull name, const Elf_Sym &sym, Library *_Nonnull lib);
        /// Overrides a symbol's address.
        void overrideSymbol(const SymbolMap::Symbol * _Nonnull inSym, const uintptr_t newAddr);

        /// Registers the main executable's TLS requirements.
        void setExecTlsRequirements(const size_t totalLen, const std::span<std::byte> &tdata);
        /// Registers a library's thread-local requirements.
        void setLibTlsRequirements(const size_t totalLen, const std::span<std::byte> &tdata,
                Library * _Nonnull library);


        ThreadLocal * _Nonnull getTls() {
            return this->tls;
        }
        DlInfo * _Nonnull getDlInfo() {
            return this->dlInfo;
        }

    private:
        void secondInit();

        /// Load a shared library
        void loadSharedLib(const char * _Nonnull soname);
        /// Searches for a library with the given name in system paths and open it
        FILE * _Nullable openSharedLib(const char * _Nonnull soname, const char * _Nullable &outPath);

    private:
        /// shared linker instance
        static Linker * _Nullable gShared;
        /// are trace logs enabled?
        static bool gLogTraceEnabled;

        /// are we logging the files we open?
        static bool gLogOpenAttempts;
        /// whether we log initializer/destructor invocations
        static bool gLogInitFini;
        /// are we logging thread-local info
        static bool gLogTls;

        /// Whether we output logs for each library we fix up
        constexpr static const bool kLogLibraryFixups{false};

    private:
        /// path from which the file is loaded
        const char * _Nonnull path;

        /// ELF reader for the executable
        ElfExecReader * _Nullable exec = nullptr;
        /// thread local information
        ThreadLocal *_Nonnull tls;
        /// dynamic linker runtime functions
        DlInfo *_Nonnull dlInfo;

        /// base address to load the next shared library at
        uintptr_t soBase = kSharedLibBase;
        /// memory address holding program entry point
        uintptr_t entryAddr = 0;

        /// executable initializer functions
        std::list<void(*)(void)> execInitFuncs;
        /// executable termination functions
        std::list<void(*)(void)> execFiniFuncs;

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
