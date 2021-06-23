#pragma once

#include <cstddef>
#include <memory>

#include <DriverSupport/disk/Client.h>

/**
 * Abstract interface for a filesystem
 */
class Filesystem {
    public:
        Filesystem(const std::shared_ptr<DriverSupport::disk::Disk> &_disk) : disk(_disk) {}
        virtual ~Filesystem() = default;

    protected:
        /// Disk on which this filesystem resides
        std::shared_ptr<DriverSupport::disk::Disk> disk;
};
