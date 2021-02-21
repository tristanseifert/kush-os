#ifndef TASK_LOADER_ELF32_H
#define TASK_LOADER_ELF32_H

#include "Loader.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace task::loader {
class Elf32: public Loader {
    public:
        Elf32(const std::span<std::byte> &bytes);
};
}

#endif
