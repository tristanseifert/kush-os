#ifndef KERNEL_SCHED_PEERLIST_H
#define KERNEL_SCHED_PEERLIST_H

#include <stddef.h>

#include <runtime/Vector.h>

namespace sched {
class Scheduler;

/**
 * The peer list is a sorted list of schedulers, in increasing "distance" from the current core. It
 * is used by the scheduler when work stealing and determining whether a thread should be
 * migrated.
 *
 * Additionally, when the object is created/destroyed, it will automatically handle registering the
 * scheduler in the required global structures.
 */
class PeerList {
    friend class Scheduler;

    public:
        PeerList(Scheduler *_sched);
        ~PeerList();

        /**
         * Invalidates this peer list, so that it is recomputed the next time the scheduler is
         * idle.
         */
        inline void invalidate() {
            __atomic_store_n(&this->dirty, true, __ATOMIC_RELEASE);
        }

        /**
         * Rebuilds the peer list if it's dirtied.
         */
        void rebuild() {
            if(__atomic_load_n(&this->dirty, __ATOMIC_RELAXED)) {
                this->build();
            }
        }

    private:
        /**
         * Describes information on a particular core's scheduler instance. This consists of a
         * pointer to its scheduler instance as well as some core identification information.
         *
         * This is primarily accessed by idle cores when looking for work to steal. Each core's
         * local scheduler will build a list of cores to steal from, in ascending order of some
         * platform defined "cost" of migrating a thread off of that source core. This allows us
         * to be aware of things like cache structures, SMP, and so forth.
         */
        struct InstanceInfo {
            /// core ID (platform specific)
            uintptr_t coreId = 0;
            /// scheduler running on this core
            Scheduler *instance = nullptr;

            InstanceInfo() = default;
            InstanceInfo(Scheduler *_instance) : instance(_instance) {}
        };

    private:
        /// Builds the peer list based on the currently active schedulers
        void build();
        /// Invalidates all other peer lists in the system.
        void invalidateOthers();

    private:
        /// all schedulers on the system. used for work stealing
        static rt::Vector<InstanceInfo> *gSchedulers;

    private:
        /// scheduler instance that owns us
        Scheduler *owner;

        /**
         * List of other cores' schedulers, ordered by ascending cost of migrating a thread from
         * that core. This list is used when looking for work to steal when we become idle.
         *
         * The list is built lazily when we're idle and the dirty flag is set.
         */
        rt::Vector<InstanceInfo> peers;
        /// when set, the peer map is dirty and must be updated
        bool dirty = true;
};
}

#endif
