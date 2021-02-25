#ifndef DYLDO_H
#define DYLDO_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "struct/hashmap.h"

namespace dyldo {
class ElfReader;
class ElfExecReader;
class ElfLibReader;

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
        /// Outputs a message if trace logging is enabled
        static void Trace(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
        /// Outputs an informational message
        static void Info(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
        /// Outputs an error message and exits the task.
        [[noreturn]] static void Abort(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));

    public:
        Linker(const char * _Nonnull path);

        void loadLibs();
        void cleanUp();

    private:
        /**
         * Information for a loaded library. This consists of the virtual address at which it was
         * loaded.
         */
        struct Library {
            /// base address
            uintptr_t base = 0;
            /// ELF reader for the library
            ElfLibReader * _Nullable reader = nullptr;
        };

    private:
        /// Load a shared library
        void loadSharedLib(const char * _Nonnull soname);
        /// Searches for a library with the given name in system paths and open it
        FILE * _Nullable openSharedLib(const char * _Nonnull soname);

    private:
        /// are trace logs enabled?
        static bool gLogTraceEnabled;

    private:
        /// ELF reader for the executable
        ElfExecReader * _Nullable exec = nullptr;

        /// base address to load the next shared library at
        uintptr_t soBase = kSharedLibBase;

        /**
         * Map of libraries loaded; this is used exclusively during the loading and linking
         * process and is deallocated when the app is about to be launched. Therefore, the keys do
         * not need to be valid past that time.
         */
        struct hashmap_s loaded;
};
}

#endif
