#ifndef TASK_INFOPAGE_H
#define TASK_INFOPAGE_H

#include <cstddef>
#include <cstdint>
#include <memory>

struct __system_info;

namespace dispensary {
class RpcHandler;
}

namespace task {
class Task;

/**
 * Manages the info page, which is a region of shared memory mapped into all tasks providing some
 * information about the environment to allow tasks to bootstrap themselves.
 *
 * Despite the name, this may actually occupy more than one page of memory.
 */
class InfoPage {
    friend class Task;
    friend class dispensary::RpcHandler;

    public:
#if defined(__i386__)
        constexpr static const uintptr_t kBaseAddr = 0xBF5FE000;
        constexpr static const size_t kPageLength = 0x1000;
#elif defined(__amd64__)
        constexpr static const uintptr_t kBaseAddr = 0x00007FFF00800000;
        constexpr static const size_t kPageLength = 0x1000;
#else
#error Update InfoPage with arch details
#endif

    public:
        /// Initializes the shared info page
        static void init() {
            gShared = new InfoPage;
        }

    private:
        InfoPage();

        void mapInto(std::shared_ptr<Task> &task);

    private:
        static InfoPage *gShared;

    private:
        /// VM region handle of the page
        uintptr_t vmHandle;
        /// base of sysinfo page
        struct __system_info *info = nullptr;
};
}

#endif
