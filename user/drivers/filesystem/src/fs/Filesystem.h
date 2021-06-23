#pragma once

#include "Directory.h"

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
