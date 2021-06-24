#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * Abstract interface for a file object. This provides an interface to do random IO on a file on
 * disk, and can implement filesystem specific semantics easily.
 *
 * Closing the file is accomplished by deallocating the object.
 */
class FileBase {
    public:
        /**
         * Close the file. You should release any filesystem specific memory, flush buffers, and
         * clear caches associated with the file if relevant.
         */
        virtual ~FileBase() = default;

        /// Get the file's name
        virtual const std::string &getName() const = 0;
        /// Get the full (filesystem specific) path of this file
        virtual const std::string &getFullPath() const = 0;

        /// Size, in bytes
        virtual uint64_t getFileSize() const = 0;

        /**
         * Read the provided range of bytes from the file. Reads that would go past the end of the
         * file should be truncated to a valid range, or an empty range.
         *
         * @return 0 on success, or an error code.
         */
        virtual int read(const uint64_t offset, const size_t numBytes,
                std::vector<std::byte> &readBuf) = 0;
};
