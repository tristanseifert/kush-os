#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "Server_Filesystem.hpp"

class FileBase;
class LegacyIo;

/**
 * Implements the filesystem "message loop" which handles RPC calls to the Filesystem endpoint,
 * which in turn is used for all file IO.
 *
 * @note Unlike normal RPC servers, it is possible we'll be calling the `implX` methods from
 * multiple threads at once, as there's another server running the legacy IO. This means that any
 * shared data structures need to be adequately protected.
 */
class MessageLoop: public rpc::FilesystemServer {
    friend class LegacyIo;

    /// Name under which the message loop's port is registered
    constexpr static const std::string_view kPortName{"me.blraaz.rpc.vfs"};

    public:
        /// Filesystem RPC errors
        enum Errors: int {
            /// The provided handle is invalid
            InvalidFileHandle           = -66050,
            /// We encountered an internal error during the IO operation
            InternalError               = -66051,
        };

    public:
        MessageLoop();
        ~MessageLoop();

    protected:
        OpenFileReturn implOpenFile(const std::string &path, uint32_t mode) override;
        SlowReadReturn implSlowRead(uint64_t handle, uint64_t offset, uint16_t numBytes) override;
        int32_t implCloseFile(uint64_t handle) override;

    private:
        void legacyWorkerMain();

    private:
        /// Whether file open/close is logged
        constexpr static const bool kLogOpen{false};
        /// Whether file IO is logged
        constexpr static const bool kLogIo{false};

        /// Value for the next file handle
        std::atomic_uint64_t nextFileHandle{1};

        /// File handle -> file object map
        std::unordered_map<uint64_t, std::shared_ptr<FileBase>> openFiles;
        /// Lock protecting the map
        std::mutex openFilesLock;

        /// legacy IO handler
        std::unique_ptr<LegacyIo> legacy;
};
