#include <platform.h>

#include <log.h>

/**
 * Raises the interrupt priority level of the current processor. The previous irql is returned.
 */
platform::Irql platform_raise_irql(const platform::Irql irql, const bool enableIrq) {
    panic("%s unimplemented", __PRETTY_FUNCTION__);
}

/**
 * Lowers the interrupt priority level of the current processor.
 */
void platform_lower_irql(const platform::Irql irql, const bool enableIrq) {
    panic("%s unimplemented", __PRETTY_FUNCTION__);
}

/**
 * Gets the current irql of the processor.
 */
const platform::Irql platform_get_irql() {
    panic("%s unimplemented", __PRETTY_FUNCTION__);
}
