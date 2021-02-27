#ifndef DYLDO_RUNTIME_THREADLOCAL_H
#define DYLDO_RUNTIME_THREADLOCAL_H

#include <cstdint>
#include <span>

/// exported private API
extern "C" const size_t __dyldo_get_tls_info(void * _Nullable * _Nullable outData,
        size_t * _Nullable outDataLen);
extern "C" void * _Nullable __dyldo_setup_tls();
extern "C" void __dyldo_teardown_tls();

namespace dyldo {
/**
 * Handles management of thread-local data.
 *
 * The C runtime will invoke methods in this class (exported via pseudo symbols) to set up new
 * threads it creates; and we'll make sure the main thread's TLS section is set up properly before
 * calling any code in the executables or libraries.
 */
class ThreadLocal {
    friend const size_t __dyldo_get_tls_info(void * _Nullable * _Nullable, size_t * _Nullable);
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

    public:
        ThreadLocal();

        /// Sets the size of the executable TLS region.
        void setExecTlsInfo(const size_t size, const std::span<std::byte> &tdata);

        /// Set up the calling thread's thread-local storage.
        void * _Nullable setUp();
        /// Tears down the calling thread's TLS and releases its memory.
        void tearDown();

        /**
         * Base values for initializes TLS section. This array should be copied to the start of all
         * new TLS sections.
         */
        std::span<std::byte> tdata;

        /**
         * Total length of the TLS section, INCLUDING the tdata section. All memory beyond the
         * initialized section should be zeroed.
         */
        size_t totalSize = 0;

    private:
        void updateThreadArchState(TlsBlock * _Nullable);
};
}

#endif
