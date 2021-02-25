#ifndef INIT_BUNDLEFILERPCHANDLER_H
#define INIT_BUNDLEFILERPCHANDLER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <span>
#include <string_view>
#include <thread>

#include "Bundle.h"

namespace rpc {
struct RpcPacket;
enum class FileIoEpType: uint32_t;
}

namespace init {
/**
 * Provides a file IO RPC interface backed by the init bundle. This allows tasks during early init
 * to read from the init bundle's contents.
 *
 * Since this is only used during init phase of the system, we don't implement observation on
 * tasks to ensure the handles are closed. This can result in us leaking the memory used by the
 * init bundle if an init server crashes, but at that point the system is probably pretty fucked
 * anyways.
 */
class BundleFileRpcHandler {
    public:
        /// Name of the service to register
        static const std::string_view kPortName;
        /// maximum length of messages to be received by this handler; this includes headers
        constexpr static const size_t kMaxMsgLen = (1024 * 16);

        /// maximum IO block size
        constexpr static const size_t kMaxBlockSize = (4096 * 8);

    public:
        BundleFileRpcHandler(std::shared_ptr<Bundle> &_bundle);

        /// Initializes the shared instance of the file RPC provider.
        static void init(std::shared_ptr<Bundle> &bundle) {
            gShared = new BundleFileRpcHandler(bundle);
        }

    private:
        /// Opened file handle info
        struct OpenedFile {
            /// the file object
            std::shared_ptr<Bundle::File> file;
            /// task and thread handles that opened it
            uintptr_t ownerTask, ownerThread;

            OpenedFile(std::shared_ptr<Bundle::File> &_file, const uintptr_t task,
                    const uintptr_t thread) : file(_file), ownerTask(task), ownerThread(thread) {};
        };

    private:
        /// thread entry point
        void main();

        /// processes the open() request
        void handleGetCaps(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);
        void handleOpen(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);
        void openFailed(const int, const rpc::RpcPacket *);
        void handleReadDirect(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);
        void readFailed(const uintptr_t, const int, const rpc::RpcPacket *);

        void handleClose(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);

        void reply(const rpc::RpcPacket *packet, const rpc::FileIoEpType type,
        const std::span<uint8_t> &buf);

    private:
        static BundleFileRpcHandler *gShared;

        // bundle that backs this handler
        std::shared_ptr<Bundle> bundle;

        /// message port
        uintptr_t portHandle = 0;
        /// as long as this set, the worker will continue execute
        std::atomic_bool run;
        /// runloop thread
        std::unique_ptr<std::thread> worker;

        /// next file handle
        std::atomic_uintptr_t nextHandle = 1;
        /// file handles
        std::unordered_map<uintptr_t, OpenedFile> openFiles;
};
}

#endif
