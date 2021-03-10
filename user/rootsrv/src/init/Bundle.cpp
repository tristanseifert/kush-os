#include "Bundle.h"
#include "BundleTypes.h"
#include "tar.h"

#include "StringHelpers.h"

#include <cstring>

#include <sys/syscalls.h>
#include <compress/lzfse.h>

#include "log.h"

using namespace init;

/**
 * Creates an init bundle reader, with the given base address.
 *
 * @note It's assumed that the entire bundle is mapped in. In any case, we confirm the length of
 * the VM region against the value we read out of the header during validation.
 */
Bundle::Bundle(const uintptr_t _base) : base(reinterpret_cast<void *>(_base)) {
    int err;

    // translate the VM address to a region handle
    err = VirtualGetHandleForAddr(_base, &this->baseHandle);
    REQUIRE(err == 1, "failed to get bundle VM handle: %d", err);

    // get region size
    err = VirtualRegionGetInfo(this->baseHandle, nullptr, &this->size, nullptr);
    REQUIRE(!err, "failed to get bundle vm region info: %d", err);
}

/**
 * Validate the init bundle. We assume that the file we got is a valid tarball.
 */
bool Bundle::validate() {
    return true;
}

/**
 * Helper method to convert an octal string to a binary number.
 */
static size_t Oct2Bin(const char *str, size_t size) {
    size_t n = 0;
    auto c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

/**
 * Searches the bundle for the given file. This can have the appearance of having paths, but we
 * just do a string comparison.
 *
 * @return A file object (which must be `delete`d when done with) or nullptr, if file doesn't exist
 */
std::shared_ptr<Bundle::File> Bundle::open(const std::string &_name) {
    auto name = _name;
    name = trim(name);

    // return immediately if it's in the cache
    if(this->fileCache.contains(name)) {
        auto ptr = this->fileCache.at(name).lock();

        // return the pointer
        if(ptr) {
            return ptr;
        } 
        // pointer expired, so remove it from cache
        else {
            this->fileCache.erase(name);
        }
    }

    // iterate over all file entries
    auto read = reinterpret_cast<uint8_t *>(this->base);
    const auto bundleEnd = reinterpret_cast<uintptr_t>(read) + this->size;

    // iterate until we find the right header
    while((reinterpret_cast<uintptr_t>(read) + 512) < bundleEnd &&
            !memcmp(read + 257, TMAGIC, 5)) {
        // get the size of this header and entry
        auto hdr = reinterpret_cast<struct posix_header *>(read);
        const auto size = Oct2Bin(hdr->size, 11);

        // compare filename
        if(!strncmp(hdr->name, name.c_str(), name.length())) {
            auto file = std::make_shared<File>(this->base, hdr);
            // store it in our cache
            this->fileCache[name] = file;
            // done
            return file;
        }

        // advance to next entry
        read += (((size + 511) / 512) + 1) * 512;
    }

    // if we get here, the file was not found
    return nullptr;
}



/**
 * Given a file header and base pointer, creates a file object to represent it.
 */
Bundle::File::File(void *base, const struct posix_header *hdr) {
    // copy the file name
    this->name = std::string(hdr->name, strlen(hdr->name));

    // construct span for the contents directly if not compressed
    const auto size = Oct2Bin(hdr->size, 11);
    auto data = reinterpret_cast<std::byte *>(((uintptr_t) hdr) + 512);
    this->contents = std::span(data, size);
}
