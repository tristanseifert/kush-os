#ifndef INIT_BUNDLE_H
#define INIT_BUNDLE_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <string>
#include <span>

struct InitHeader;
struct InitFileHeader;

namespace init {
/**
 * Provides access to an in-memory init bundle.
 */
class Bundle {
    public:
        /// Virtual load address of init bundle
#if defined(__i386__)
        constexpr static const uintptr_t kBundleAddr = 0x90000000;
#elif defined(__amd64__)
        constexpr static const uintptr_t kBundleAddr = 0x000000dead000000;
#else
#error Update init::Bundle::kBundleAddr for this architecture!
#endif

    public:
        class File {
            friend class Bundle;

            public:
                /// Returns the name of the file
                const std::string &getName() const {
                    return this->name;
                }

                /// Returns the size of the file.
                const size_t getSize() const {
                    return this->contents.size_bytes();
                }
                /// Gets the file's contents.
                const std::span<std::byte> &getContents() const {
                    return this->contents;
                }

                ~File() {
                    if(this->decompressed) {
                        delete[] this->decompressed;
                    }
                }

                // you should not call this; only public for std::shared_ptr
                File(void *base, const InitFileHeader *hdr);

            private:
                std::string name;
                std::span<std::byte> contents;

                /// if non-null, this is a byte buffer we need to deallocate when released
                std::byte *decompressed = nullptr;
        };

    public:
        /// allocates the structures for a bundle at the given VM address
        Bundle(const uintptr_t vmBase = kBundleAddr);
        /// ensures the bundle header is valid
        bool validate();

        /// Opens a file with the given name.
        std::shared_ptr<File> open(const std::string &name);

    private:
        /// base address of the init bundle
        void *base = nullptr;
        /// handle to the VM region containing the init bundle
        uintptr_t baseHandle = 0;

        /// once validated, a pointer to the bundle's header
        const InitHeader *header = nullptr;

        /// cache of opened files
        std::unordered_map<std::string, std::weak_ptr<File>> fileCache;
};
}

#endif
