#ifndef EXPERT_EXPERT_H
#define EXPERT_EXPERT_H

#include <cstdint>
#include <string_view>

/**
 * Base class for all platform experts
 */
class Expert {
    public:
        virtual ~Expert() = default;

        /// Performs a probe for available devices.
        virtual void probe() = 0;

    public:
        /// Create an expert from name.
        static Expert * _Nullable create(const std::string_view &name);
};

#endif
