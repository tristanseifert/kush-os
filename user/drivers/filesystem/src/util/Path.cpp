#include "Path.h"

#include <cstddef>
#include <string>

using namespace util;

/// Path separator character
constexpr static const char kPathSeparator{'/'};

/**
 * Splits the path into its individual components. The path should be absolute (that is, have a
 * leading slash) for this to work right. No escaping of slashes is allowed; they are always
 * interpreted as a path separator.
 */
void util::SplitPath(const std::string &path,
        std::vector<std::string> &outComponents) {
    size_t start{0};
    size_t end{0};

    while ((start = path.find_first_not_of(kPathSeparator, end)) != std::string::npos) {
        end = path.find(kPathSeparator, start);
        outComponents.push_back(path.substr(start, end - start));
    }
}

