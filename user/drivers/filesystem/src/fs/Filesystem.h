#pragma once

#include "Directory.h"
#include "File.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include <DriverSupport/disk/Client.h>

/**
 * Abstract interface for a filesystem
 */
class Filesystem {
    public:
        Filesystem(const std::shared_ptr<DriverSupport::disk::Disk> &_disk) : disk(_disk) {}
        virtual ~Filesystem() = default;

        /// Filesystem error codes
        enum Errors: int {
            /// File was not found
            FileNotFound                = -66100,
            /// The directory entry type is invalid for this call
            InvalidDirentType           = -66101,
        };

        /**
         * Reads the contents of a directory represented by the given directory entry.
         *
         * @param dent A directory type entry that we're to read the contents of
         * @param out Pointer to store the read directory in, on success.
         *
         * @return 0 on success, or an error code.
         */
        virtual int readDirectory(DirectoryEntryBase * _Nonnull dent,
                std::shared_ptr<DirectoryBase> &out) = 0;

        /**
         * Opens a directory entry as a file for IO. The created file object is closed when all
         * references to it are dropped.
         *
         * @return 0 on success, error code otherwise.
         */
        virtual int openFile(DirectoryEntryBase * _Nonnull dent,
                std::shared_ptr<FileBase> &out) = 0;

        /**
         * Returns the root directory on the filesystem. This is the only directory that the driver
         * is _required_ to cache and have accessible at all times.
         */
        virtual std::shared_ptr<DirectoryBase> getRootDirectory() const = 0;

        /**
         * If the filesystem supports it, return the user specified "volume label" attached to this
         * instance.
         */
        virtual std::optional<std::string> getVolumeLabel() const {
            return std::nullopt;
        }

    protected:
        /// Disk on which this filesystem resides
        std::shared_ptr<DriverSupport::disk::Disk> disk;
};
