#include "Bundle.h"
#include "BundleTypes.h"

#include "StringHelpers.h"

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
}

/**
 * Validates the init bundle. This is done in a few steps:
 *
 * - Read the magic value from the header, followed by the version field. These must match the
 *   expected value, and be a 1.x version. Then, we'll ensure that the type field indicates that
 *   we've loaded an init bundle.
 * - Check the header fields for sanity. We'll read out the expected length of the init bundle
 *   here, and compare it against what the system tells us the VM region is.
 */
bool Bundle::validate() {
    int err;

    // 1. read header
    auto hdr = reinterpret_cast<const InitHeader *>(this->base);
    if(hdr->magic != kInitMagic) {
        PANIC("invalid bundle magic $%08x", hdr->magic);
    }
    else if(hdr->major != 1) {
        PANIC("unsupported bundle version maj $%04x min $%04x", hdr->major, hdr->minor);
    }
    else if(hdr->type != kInitType) {
        PANIC("invalid bundle type $%08x", hdr->type);
    }

    // 2. validate header size
    uintptr_t regionSize;
    err = VirtualRegionGetInfo(this->baseHandle, nullptr, &regionSize, nullptr);
    REQUIRE(!err, "failed to get bundle vm region info: %d", err);

    if(hdr->totalLen > regionSize || hdr->totalLen <= sizeof(InitHeader)) {
        PANIC("bundle size invalid: header %d, region %d", hdr->totalLen, regionSize);
    }

    // if we get here, the bundle must've been valid
    this->header = hdr;
    return true;
}

/**
 * Searches the bundle for the given file. This can have the appearance of having paths, but we
 * just do a string comparison.
 *
 * @return A file object (which must be `delete`d when done with) or nullptr, if file doesn't exist
 */
std::shared_ptr<Bundle::File> Bundle::open(const std::string &_name) {
    // trim the filename
    const InitFileHeader *fileHdr = this->header->headers;
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
    for(size_t i = 0; i < this->header->numFiles; i++) {
        // compare the names
        std::string thisName(fileHdr->name, static_cast<size_t>(fileHdr->nameLen));
        if(name == thisName) {
            auto file = std::make_shared<File>(this->base, fileHdr);

            // store it in our cache
            this->fileCache[name] = file;

            // done
            return file;
        }

        // advance to the next header
        fileHdr = reinterpret_cast<InitFileHeader *>(((uintptr_t) fileHdr) + sizeof(InitFileHeader) + fileHdr->nameLen);
    }

    // if we get here, the file was not found
    return nullptr;
}



/**
 * Given a file header and base pointer, creates a file object to represent it.
 */
Bundle::File::File(void *base, const InitFileHeader *hdr) {
    // copy the file name
    this->name = std::string(hdr->name, (size_t) hdr->nameLen);

    // construct span for the contents directly if not compressed
    if(!(hdr->flags & kInitFileFlagsCompressed)) {
        auto data = reinterpret_cast<std::byte *>(((uintptr_t) base) + hdr->dataOff);
        this->contents = std::span(data, hdr->dataLen);
    } 
    // otherwise, allocate a buffer and decompress into it
    else {
        // allocate the buffer
        this->decompressed = new std::byte[hdr->rawLen];
        REQUIRE(this->decompressed, "failed to allocate decompression buffer");

        // perform decompression
        auto compressed = reinterpret_cast<uint8_t *>(((uintptr_t) base) + hdr->dataOff);
        auto bytes = lzfse_decode_buffer(reinterpret_cast<uint8_t *>(this->decompressed),
                hdr->rawLen, compressed, hdr->dataLen, nullptr);

        REQUIRE(bytes == hdr->rawLen, "failed to decompress file %s: raw length %u, decompressed %u",
                this->name.c_str(), hdr->rawLen, bytes);

        // create a span to wrap it
        this->contents = std::span(this->decompressed, bytes);
    }
}
