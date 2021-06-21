#ifndef DRIVERSUPPORT_DISK_CLIENT_H
#define DRIVERSUPPORT_DISK_CLIENT_H

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <DriverSupport/disk/Types.h>
#include <DriverSupport/disk/Client_DiskDriver.hpp>

namespace DriverSupport::disk {
/**
 * Provides an interface to a disk.
 */
class Disk: public rpc::DiskDriverClient {
    /// Name of the device property that contains information on how to talk to the disk
    constexpr static const std::string_view kConnectionPropertyName{"disk.ata.connection"};

    public:
        enum Errors: int {
            /// The specified path is invalid
            InvalidPath                         = -40001,
            /// Failed to decode the connection info
            InvalidConnectionInfo               = -40002,
        };

    public:
        [[nodiscard]] static int Alloc(const std::string_view &forestPath,
                std::shared_ptr<Disk> &outDisk);

        virtual ~Disk();

        /// Return the capacity of the disk.
        int GetCapacity(std::pair<uint32_t, uint64_t> &outCapacity);
        /// Performs a read from disk
        int Read(const uint64_t sector, const size_t numSectors, std::vector<std::byte> &out);

    private:
        Disk(const std::shared_ptr<IoStream> &io, const std::string_view &forestPath,
                const uint64_t diskId);

        static std::pair<uintptr_t, uint64_t> DecodeConnectionInfo(const std::span<std::byte> &);

        using DiskDriverClient::GetCapacity;
        using DiskDriverClient::OpenSession;
        using DiskDriverClient::CloseSession;

    private:
        int status{0};

        /// Forest path of this disk
        std::string forestPath;
        /// ID for this disk to use in RPC calls
        uint64_t id{0};

        /// Session token
        uint64_t sessionToken{0};

        /// VM handle of the command descriptor region
        uintptr_t commandVmRegion{0};
        /// Base address of the command descriptor region
        volatile Command *commandList{nullptr};
        /// Total number of available commands
        size_t numCommands{0};
};
};

#endif
