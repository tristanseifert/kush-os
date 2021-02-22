#ifndef INIT_BUNDLEFILERPCHANDLER_H
#define INIT_BUNDLEFILERPCHANDLER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>
#include <thread>

namespace init {
class Bundle;

/**
 * Provides a file IO RPC interface backed by the init bundle. This allows tasks during early init
 * to read from the init bundle's contents.
 */
class BundleFileRpcHandler {
    public:
        /// Name of the service to register
        static const std::string_view kPortName;
        /// maximum length of messages to be received by this handler; this includes headers
        constexpr static const size_t kMaxMsgLen = (1024 * 16);

    public:
        BundleFileRpcHandler(std::shared_ptr<Bundle> &_bundle);

        /// Initializes the shared instance of the file RPC provider.
        static void init(std::shared_ptr<Bundle> &bundle) {
            gShared = new BundleFileRpcHandler(bundle);
        }

    private:
        /// thread entry point
        void main();

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
};
}

#endif
