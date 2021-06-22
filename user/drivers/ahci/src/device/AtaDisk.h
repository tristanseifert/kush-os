#ifndef AHCIDRV_DEVICE_ATADISK_H
#define AHCIDRV_DEVICE_ATADISK_H

#include "Device.h"
#include "Port.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <driver/DmaBuffer.h>
#include <driver/ScatterGatherBuffer.h>

/**
 * Provides an interface for an ATA hard drive type device, whether it is connected via parallel
 * or SATA.
 */
class AtaDisk: public Device, public std::enable_shared_from_this<AtaDisk> {
    using DMABufferPtr = std::shared_ptr<libdriver::DmaBuffer>;

    /// Device name for ATA disk
    constexpr static const std::string_view kDeviceName{"AtaDisk,GenericDisk"};

    /// Name of the device property that contains information about the disk
    constexpr static const std::string_view kInfoPropertyName{"disk.ata.info"};
    /// Name of the device property that contains information on how to talk to the disk
    constexpr static const std::string_view kConnectionPropertyName{"disk.ata.connection"};

    public:
        [[nodiscard]] static int Alloc(const std::shared_ptr<Port> &port,
                std::shared_ptr<AtaDisk> &outDisk);

        virtual ~AtaDisk();

        /// Performs a read that fills the given buffer.
        [[nodiscard]] int read(const uint64_t start, const size_t numSectors, const DMABufferPtr &to,
                const std::function<void(bool)> &callback);

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

        // don't use this
        AtaDisk(const std::shared_ptr<Port> &port);

    private:
        void identify();
        void invalidate();
        void registerDisk();

        void startRpc();
        void stopRpc();

        void handleIdentifyResponse(const Port::CommandResult &);
        void identifyExtractStrings(const std::span<std::byte> &);
        void identifyDetermineSize(const std::span<std::byte> &);

        void serializeInfoData(std::vector<std::byte> &, const std::shared_ptr<Port> &);
        void serializeConnectionData(std::vector<std::byte> &, const std::shared_ptr<Port> &);

    private:
        /// Desired size for the scatter gather buffer
        constexpr static const size_t kSmallBufSize{2048};

        /// Whether info read from the device during initialization is logged
        constexpr static const bool kLogInfo{false};

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
        uint32_t sectorSize{512};
        /// Total number of user accessible sectors on the device
        uint64_t numSectors{0};

        /// RPC disk identifier
        uint64_t rpcId{~0U};

        /// DMA buffer for small device commands
        std::shared_ptr<libdriver::ScatterGatherBuffer> smallBuf;
};

#endif
