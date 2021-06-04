#include "Expert.h"

#include "X86PcExpert.h"
#include "Amd64PcExpert.h"

/**
 * Instantiates an expert based on its name.
 */
Expert *Expert::create(const std::string_view &name) {
#ifdef __i386__
    if(name == "pc_x86") {
        return new X86PcExpert;
    }
#endif
#ifdef __amd64__
    if(name == "pc_amd64") {
        return new Amd64PcExpert;
    }
#endif

    // no suitable expert
    return nullptr;
}
