#pragma once

#include <string>
#include <vector>

namespace util {
/**
 * Splits a path string into its components.
 */
void SplitPath(const std::string &path, std::vector<std::string> &outComponents);
};
