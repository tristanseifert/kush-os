#include <platform.h>

#include <log.h>

/**
 * Registers a new timer callback.
 *
 * The given function is invoked at (or after -- there is no guarantee made as to the resolution of
 * the underlying hardware timers) the given time, in nanoseconds since system boot.
 *
 * Returned is an opaque token that can be used to cancel the timer before it fires. Timers are
 * always one shot.
 *
 * @note The callbacks may be invoked in an interrupt context; you should do as little work as
 * possible there.
 *
 * @return An opaque timer token, or 0 in case an error occurs.
 */
uintptr_t platform_timer_add(const uint64_t at, void (*callback)(const uintptr_t, void *), void *ctx) {
    log("%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}

/**
 * Removes a previously created timer, if it has not fired yet.
 */
void platform_timer_remove(const uintptr_t token) {
    log("%s unimplemented", __PRETTY_FUNCTION__);
}

