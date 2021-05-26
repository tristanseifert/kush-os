#ifndef KERNEL_SCHED_DEADLINE_H
#define KERNEL_SCHED_DEADLINE_H

#include <cstdint>

namespace sched {
struct Thread;

/**
 * Deadlines represent some sort of action unrelated to the regular flow of user code, at some time
 * in the future. This includes things like sleeping a thread, timed waits, etc.
 *
 * Each deadline consists of an absolute time at which it becomes due, and a function that is
 * invoked at that time. Other parts of the system can subclass this object to implement custom
 * behavior on arrival of the deadline.
 */
struct Deadline {
    Deadline() = default;
    Deadline(const uint64_t _expires) : expires(_expires) {}
    virtual ~Deadline() = default;

    /**
     * Absolute time (in nanoseconds) at which the deadline expires, and its call method will be
     * invoked.
     */
    uint64_t expires = 0;

    /**
     * This operator is invoked when the deadline expires; keep in mind that this will be invoked
     * from the scheduler's timer context, so the amount of work done should be kept to a minimum,
     * such as placing a thread back on the run queue.
     */
    virtual void operator()()= 0;


    bool operator ==(const Deadline &d) const {
        return this->expires == d.expires;
    }
    bool operator !=(const Deadline &d) const {
        return this->expires != d.expires;
    }
    bool operator <(const Deadline &d) const {
        return this->expires < d.expires;
    }
    bool operator <=(const Deadline &d) const {
        return this->expires <= d.expires;
    }
    bool operator >(const Deadline &d) const {
        return this->expires > d.expires;
    }
    bool operator >=(const Deadline &d) const {
        return this->expires >= d.expires;
    }
};
}

#endif
