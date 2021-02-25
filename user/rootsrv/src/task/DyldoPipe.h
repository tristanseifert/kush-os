#ifndef TASK_DYLDOPIPE_H
#define TASK_DYLDOPIPE_H

#include <cstdint>
#include <mutex>

namespace task {
class Task;

/**
 * Communications interface between us and the dynamic linker, used for task provisioning.
 */
class DyldoPipe {
    private:
        /// size of the message buffer, in bytes
        constexpr static const size_t kMsgBufLen = 1024;
        /// dynamic linker port name
        const static std::string kDyldoPortName;

    public:
        DyldoPipe();
        ~DyldoPipe();

        /// A new task has been launched; notify linker and get the dynamic linker entry point
        int taskLaunched(Task *t, uintptr_t &linkerEntry);

    private:
        /// message buffer
        void *msgBuf = nullptr;

        /// lock to ensure only one task accesses us at a time
        std::mutex lock;

        /// port of the dynamic linker
        uintptr_t dyldoPort;
        /// port handle of our receive port
        uintptr_t replyPort;
};
}

#endif
