#include "Expert.h"

#if defined(__i386__)
#include "X86PcExpert.h"
#endif

/**
 * Instantiates an expert based on its name.
 */
Expert *Expert::create(const std::string_view &name) {
#if defined(__i386__)
    if(name == "pc_x86") {
        return new X86PcExpert;
    }
#endif

    // no suitable expert
    return nullptr;
}
