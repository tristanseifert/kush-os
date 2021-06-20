#ifndef AHCIDRV_DEVICE_ATADISK_H
#define AHCIDRV_DEVICE_ATADISK_H

#include "Device.h"
#include "Port.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <driver/ScatterGatherBuffer.h>

/**
 * Provides an interface for an ATA hard drive type device, whether it is connected via parallel
 * or SATA.
 */
class AtaDisk: public Device {
    using DMABufferPtr = std::shared_ptr<libdriver::ScatterGatherBuffer>;

    /// Name of the device property that contains information about the disk
    constexpr static const std::string_view kInfoPropertyName{"disk.ata.info"};
    /// Name of the device property that contains information on how to talk to the disk
    constexpr static const std::string_view kConnectionPropertyName{"disk.ata.connection"};

    public:
        [[nodiscard]] static int Alloc(const std::shared_ptr<Port> &port,
                std::shared_ptr<AtaDisk> &outDisk);

        virtual ~AtaDisk();

        /// Current status of the disk; 0 if valid
        constexpr auto getStatus() const {
            return this->status;
        }

        /// Get disk model name
        constexpr const std::string &getModelName() const {
            return this->model;
        }
        /// Get disk serial number
        constexpr const std::string &getDeviceSerial() const {
            return this->serial;
        }
        /// Get firmware revision
        constexpr const std::string &getFirmwareRev() const {
            return this->firmwareVersion;
        }

        /// Returns the size, in bytes, of the disk.
        constexpr auto getSize() const {
            return this->sectorSize * this->numSectors;
        }
        /// Returns the number of sectors on the disk.
        constexpr auto getNumSectors() const {
            return this->numSectors;
        }
        /// Returns the size of each sector, in bytes
        constexpr auto getSectorSize() const {
            return this->sectorSize;
        }

    private:
        AtaDisk(const std::shared_ptr<Port> &port);

        void invalidate();
        void registerDisk();

        void handleIdentifyResponse(const Port::CommandResult &);
        void identifyExtractStrings(const std::span<std::byte> &);
        void identifyDetermineSize(const std::span<std::byte> &);

        void serializeInfoData(std::vector<std::byte> &, const std::shared_ptr<Port> &);
        void serializeConnectionData(std::vector<std::byte> &, const std::shared_ptr<Port> &);

    private:
        /// Desired size for the scatter gather buffer
        constexpr static const size_t kSmallBufSize{2048};

        /// status code (if an error occurred)
        int status{0};

        /// Path in the driver forest
        std::string forestPath;

        /// Device serial number
        std::string serial;
        /// Device firmware revision string
        std::string firmwareVersion;
        /// Device model number
        std::string model;

        /// Sector size, in bytes
        size_t sectorSize{512};
        /// Total number of user accessible sectors on the device
        uint64_t numSectors{0};

        /// DMA buffer for small device commands
        DMABufferPtr smallBuf;
};

#endif
