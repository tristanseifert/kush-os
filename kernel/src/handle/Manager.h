#ifndef KERNEL_HANDLE_MANAGER_H
#define KERNEL_HANDLE_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#include <runtime/Vector.h>

#include <arch/rwlock.h>

extern "C" void kernel_init();

/// Handle type
enum class Handle: uintptr_t {};

namespace sched {
struct Task;
struct Thread;
} 

namespace handle {
/**
 * Handles are opaque identifiers, which can be passed to userspace, that represent different
 * types of kernel objects.
 *
 * Note that we do not take ownership of the objects; you're responsible for getting rid of them
 * as needed, once they're released; then freeing the handle slot.
 */
class Manager {
    friend void ::kernel_init();

    private:
        // type codes for handles
        enum Type: uint8_t {
            Task                        = 0x01,
            Thread                      = 0x02,
            Port                        = 0x03,
            VmRegion                    = 0x04,
        };

    public:
        /// Allocates a new handle for the given task.
        static Handle makeTaskHandle(sched::Task *task) {
            RW_LOCK_WRITE_GUARD(gShared->taskHandlesLock);
            return gShared->allocate(task, gShared->taskHandles, Type::Task);
        }
        /// Releases the previously allocated task handle.
        static bool releaseTaskHandle(const Handle h) {
            // validate type
            const auto type = getType(h);
            if(type != Type::Task) {
                return false;
            }

            // update the list
            RW_LOCK_WRITE_GUARD(gShared->taskHandlesLock);
            return gShared->release(h, gShared->taskHandles);
        }
        /// Returns the task that the given handle points to.
        static sched::Task *getTask(const Handle h) {
            // validate type
            const auto type = getType(h);
            if(type != Type::Task) {
                return nullptr;
            }

            RW_LOCK_READ_GUARD(gShared->taskHandlesLock);
            return gShared->get(h, gShared->taskHandles);
        }

        /// Allocates a new handle for the given thread.
        static Handle makeThreadHandle(sched::Thread *thread) {
            RW_LOCK_WRITE_GUARD(gShared->threadHandlesLock);
            return gShared->allocate(thread, gShared->threadHandles, Type::Thread);
        }
        /// Releases a previously allocated thread handle.
        static bool releaseThreadHandle(const Handle h) {
            // validate type
            const auto type = getType(h);
            if(type != Type::Thread) {
                return false;
            }

            // update the list
            RW_LOCK_WRITE_GUARD(gShared->threadHandlesLock);
            return gShared->release(h, gShared->threadHandles);
        }
        /// Returns the thread that the given handle points to.
        static sched::Thread *getThread(const Handle h) {
            // validate type
            const auto type = getType(h);
            if(type != Type::Thread) {
                return nullptr;
            }

            RW_LOCK_READ_GUARD(gShared->threadHandlesLock);
            return gShared->get(h, gShared->threadHandles);
        }
    private:
        /// wraps a handle slot with an epoch counter
        template<class T>
        struct HandleInfo {
            // pointer to allocated object, or null if it's free
            T *object = nullptr;
            // epoch counter
            uintptr_t epoch = 0;

            HandleInfo() = default;
            HandleInfo(T *_object) : object(_object) {};
        };

    private:
        /// Index mask
        constexpr static const uintptr_t kIndexMask = 0xFFFFF;
        /// Epoch mask
        constexpr static const uintptr_t kEpochMask = 0x7F;
        /// Bit position of the epoch value
        constexpr static const uintptr_t kEpochPos = 20;
        /// Type mask
        constexpr static const uintptr_t kTypeMask = 0xF;
        /// Bit position of the type value
        constexpr static const uintptr_t kTypePos = 27;

        /// Returns the index component of a handle.
        static inline uintptr_t getIndex(const Handle h) {
            return static_cast<uintptr_t>(h) & 0xFFFFF;
        }
        /// Returns the epoch counter of a handle.
        static inline uintptr_t getEpoch(const Handle h) {
            return (static_cast<uintptr_t>(h) & 0x7F00000) >> kEpochPos;
        }
        /// Returns the type code of a handle.
        static inline uint8_t getType(const Handle h) {
            return (static_cast<uintptr_t>(h) & 0x78000000) >> kTypePos;
        }

        /// Creates a new handle.
        static inline Handle makeHandle(const Type t, const uintptr_t index, const uintptr_t epoch) {
            uintptr_t handle = index & kIndexMask;
            handle |= (epoch & kEpochMask) << kEpochPos;
            handle |= (t & kTypeMask) << kTypePos;
            return static_cast<Handle>(handle);
        }

    private:
        static void init();

        Manager();

        /**
         * Allocates a new handle for the given object.
         *
         * We'll first scan the table to find available free slots; if one is found, we use it.
         * Otherwise, a new slot is added at the end of the table.
         *
         * You must hold the appropriate write lock when calling this function.
         */
        template<class T>
        Handle allocate(T *object, rt::Vector<HandleInfo<T>> &handles, const Type type) {
            // try to find a free slot
            for(size_t i = 0; i < handles.size(); i++) {
                auto &slot = handles[i];
                if(slot.object) continue;

                slot.object = object;
                return makeHandle(type, i, slot.epoch);
            }

            // allocate new slot
            HandleInfo info(object);

            const auto index = handles.size();
            handles.push_back(info);

            return makeHandle(type, index, info.epoch);
        }

        /**
         * Gets the object value of the given handle.
         */
        template<class T>
        T *get(const Handle h, const rt::Vector<HandleInfo<T>> &handles) {
            // validate index
            const auto index = getIndex(h);
            if(index >= gShared->taskHandles.size()) {
                return nullptr;
            }

            // validate the epoch
            const auto epoch = getEpoch(h);
            auto &info = handles[index];

            if((info.epoch & kEpochMask) != epoch) {
                return nullptr;
            }

            // the handle checked out; return the value
            return info.object;
        }


        /**
         * Releases the given handle slot, and increments its epoch counter so a stale handle can
         * be detected.
         *
         * This does not check the type code of the handle; it's assumed this was done by the
         * caller. We also assume the caller holds the correct write lock.
         *
         * @return Whether the handle slot was released successfully.
         */
        template<class T>
        bool release(const Handle h, rt::Vector<HandleInfo<T>> &handles) {
            // ensure size
            const auto index = getIndex(h);
            if(index >= handles.size()) {
                return false;
            }

            // validate the epoch
            const auto epoch = getEpoch(h);
            auto &info = handles[index];

            if((info.epoch & kEpochMask) != epoch) {
                return false;
            }

            // it's valid, so clear the pointer and increment the epoch
            T *null = nullptr;

            __atomic_store(&info.object, &null, __ATOMIC_RELEASE);
            __atomic_add_fetch(&info.epoch, 1, __ATOMIC_RELEASE);

            // we get here, it's gone :D
            return true;
        }

    private:
        static Manager *gShared;

    private:
        // lock for the task handles
        DECLARE_RWLOCK(taskHandlesLock);
        // storage for task handles
        rt::Vector<HandleInfo<sched::Task>> taskHandles;

        // lock for the thread handles
        DECLARE_RWLOCK(threadHandlesLock);
        // storage for thread handles
        rt::Vector<HandleInfo<sched::Thread>> threadHandles;
};
}

#endif
