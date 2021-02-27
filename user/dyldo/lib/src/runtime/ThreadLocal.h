#ifndef DYLDO_RUNTIME_THREADLOCAL_H
#define DYLDO_RUNTIME_THREADLOCAL_H

#include <cstdint>
#include <span>

#include "struct/hashmap.h"

/// exported private API
extern "C" void * _Nullable __dyldo_setup_tls();
extern "C" void __dyldo_teardown_tls();

namespace dyldo {
struct Library;

/**
 * Handles management of thread-local data.
 *
 * The C runtime will invoke methods in this class (exported via pseudo symbols) to set up new
 * threads it creates; and we'll make sure the main thread's TLS section is set up properly before
 * calling any code in the executables or libraries.
 */
class ThreadLocal {
    friend void * _Nullable __dyldo_setup_tls();
    friend void __dyldo_teardown_tls();

    /// minimum size of thread-local storage, in bytes
    constexpr static const size_t kTlsMinSize = sizeof(uintptr_t) * 1024;

    private:
        /// Thread local storage block
        struct TlsBlock {
            /// points to our own address
            void * _Nullable self = nullptr;
            /// base address of the memory block containing this
            void * _Nullable memBase = nullptr;
            /// base to the TLS
            void * _Nullable tlsBase = nullptr;
        };

        /// Registration for a library's TLS region
        struct LibTlsRegion {
            LibTlsRegion(Library * _Nonnull _library) : library(_library) {}

            /// Library that owns this region
            Library * _Nonnull library;

            /// offset (from the top of the executable TLS region) to this region
            off_t offset = 0;

            /// total size of the region
            size_t length = 0;
            /// data to copy from library to initialize it, if any
            std::span<std::byte> tdata;
        };

    public:
        ThreadLocal();

        /// Sets the size of the executable TLS region.
        void setExecTlsInfo(const size_t size, const std::span<std::byte> &tdata);
        /// Sets the TLS requirements of a library.
        void setLibTlsInfo(const size_t size, const std::span<std::byte> &tdata,
                Library * _Nonnull library);

        /// Return the TLS offset for the given library.
        off_t getLibTlsOffset(Library * _Nonnull library);

        /// Set up the calling thread's thread-local storage.
        void * _Nullable setUp();
        /// Tears down the calling thread's TLS and releases its memory.
        void tearDown();

        /// Get total bytes of TLS used by executable
        const size_t getExecSize() const {
            return this->totalExecSize;
        }

    private:
        /// whether new TLS allocations are logged
        static bool gLogAllocations;

    private:
        /**
         * Base values for the executable's initialized TLS section.
         */
        std::span<std::byte> tdata;
        /**
         * Total length of the executable's TLS section, INCLUDING the tdata section. All memory
         * beyond the initialized section should be zeroed.
         */
        size_t totalExecSize = 0;

        /**
         * Mapping of library soname to library thread-local allocation information structure
         * (LibTlsRegion)
         */
        struct hashmap_s libRegions;
        /**
         * Total bytes of thread local space required for shared libraries. This region is located
         * immediately above the executable's TLS region. It's zeroed by default, though it may
         * have data initialized from shared libraries.
         */
        size_t totalSharedSize = 0;
        /**
         * Offset (from the top of the executable's TLS region) at which the next shared library's
         * thread locals should be allocated.
         */
        off_t nextSharedOffset = 0;

    private:
        void updateThreadArchState(TlsBlock * _Nullable);
};
}

#endif
