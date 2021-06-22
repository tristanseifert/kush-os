#ifndef AHCIDRV_DEVICE_ATADISKRPCSERVER_H
#define AHCIDRV_DEVICE_ATADISKRPCSERVER_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <DriverSupport/disk/Server_DiskDriver.hpp>
#include <driver/BufferPool.h>

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
            /// Some generic error occurred while processing the request.
            InternalError               = -50005,
            /// An IO error occurred during the request
            IoError                     = -50006,
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

        /// Minimum size for the initial read buffer allocation
        constexpr static const size_t kReadBufferMinSize{1024 * 512};
        /// Maximum size of the read buffer allocation
        constexpr static const size_t kReadBufferMaxSize{1024 * 1024 * 8};

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

            /// Buffer pool to use for read buffer allocations
            std::shared_ptr<libdriver::BufferPool> readBuf;
            /// Sub-buffers in the read allocation that are active
            std::unordered_map<size_t, std::shared_ptr<libdriver::DmaBuffer>> readCommandBuffers;
        };

    private:
        AtaDiskRpcServer(const std::shared_ptr<IoStream> &, const uintptr_t);
        virtual ~AtaDiskRpcServer();

        void main();

        void processCommand(Session &, const size_t, volatile DriverSupport::disk::Command &);
        void doCmdRead(Session &, const size_t, volatile DriverSupport::disk::Command &);

        /// Mark command as successfully completed and notify remote thread
        inline void notifyCmdSuccess(volatile DriverSupport::disk::Command &cmd) {
            this->notifyCmdCompletion(cmd, 0);
        }
        /// Mark command as failed (with given error code) and notify remote thread
        inline void notifyCmdFailure(volatile DriverSupport::disk::Command &cmd, const int status) {
            return this->notifyCmdCompletion(cmd, status);
        }
        void notifyCmdCompletion(volatile DriverSupport::disk::Command &, const int);

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
