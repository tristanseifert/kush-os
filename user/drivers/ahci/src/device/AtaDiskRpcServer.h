#ifndef AHCIDRV_DEVICE_ATADISKRPCSERVER_H
#define AHCIDRV_DEVICE_ATADISKRPCSERVER_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <DriverSupport/disk/Server_DiskDriver.hpp>

class AtaDisk;

/**
 * RPC server for ATA disks.
 *
 * This is one instance that is shared among all disks created by this driver.
 */
class AtaDiskRpcServer: rpc::DiskDriverServer {
    public:
        enum Errors: int {
            /// The provided disk ID was not found
            NoSuchDisk                  = -50000,
            /// The sector base is out of range.
            InvalidSectorBase           = -50001,
            /// The number of sectors is to olong
            InvalidLength               = -50002,
            /// The operation is unsupported
            Unsupported                 = -50003,
            /// An invalid session token was specified
            InvalidSession              = -50004,
        };

    public:
        /// Returns the shared instance RPC server
        static AtaDiskRpcServer *the();

        /// Registers a new ATA disk with the RPC server and returns its id
        uint64_t add(const std::shared_ptr<AtaDisk> &disk);
        /// Removes a disk based on its id.
        bool remove(const uint64_t id);

        /// Returns the port handle that the RPC server can be reached at.
        constexpr uintptr_t getPortHandle() const {
            return this->listenPort;
        }

    protected:
        GetCapacityReturn implGetCapacity(uint64_t diskId) override;
        OpenSessionReturn implOpenSession() override;
        int32_t implCloseSession(uint64_t session) override;
        CreateReadBufferReturn implCreateReadBuffer(uint64_t session, uint64_t requestedSize) override;
        CreateWriteBufferReturn implCreateWriteBuffer(uint64_t session, uint64_t requestedSize) override;
        void implExecuteCommand(uint64_t session, uint32_t slot) override;
        void implReleaseReadCommand(uint64_t session, uint32_t slot) override;
        AllocWriteMemoryReturn implAllocWriteMemory(uint64_t session, uint64_t bytesRequested) override;

    private:
        /// Maximum number of commands to allocate per session
        constexpr static const size_t kMaxCommands{64};

        /**
         * Information on a particular session.
         */
        struct Session {
            /// VM region handle of the command region
            uintptr_t commandVmRegion{0};
            /// Command list
            volatile DriverSupport::disk::Command *commandList{nullptr};

            /// Total number of commands allocated
            size_t numCommands{kMaxCommands};
        };

    private:
        AtaDiskRpcServer(const std::shared_ptr<IoStream> &, const uintptr_t);
        virtual ~AtaDiskRpcServer();

        void main();

    private:
        /// Shared instance ATA server: created as needed
        static AtaDiskRpcServer *gShared;

        /// Port on which the RPC server listens
        uintptr_t listenPort{0};

        /// Mapping of all disks currently recorded
        std::unordered_map<uint64_t, std::weak_ptr<AtaDisk>> disks;
        /// ID to assign to the next disk
        uint64_t nextId{1};
        /// Lock against the disks map
        std::mutex disksLock;

        /// All active sessions
        std::unordered_map<uint64_t, Session> sessions;
        /// Lock against the sessions map
        std::mutex sessionsLock;
        /// ID to assign to the next session
        uint64_t nextSessionId{1};

        /// set as long as the worker shall be processing messages
        std::atomic_bool workerRun{true};
        /// worker thread
        std::unique_ptr<std::thread> worker;
};

#endif
