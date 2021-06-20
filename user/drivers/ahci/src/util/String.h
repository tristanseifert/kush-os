#ifndef AHCIDRV_UTIL_STRING
#define AHCIDRV_UTIL_STRING

#include <string>

namespace util {
/**
 * Converts an ATA string in place by swapping bytes in each word.
 */
void ConvertAtaString(std::string &str);

/**
 * Trims a string in place to remove any trailing whitespace.
 */
void TrimTrailingWhitespace(std::string &str);
}

#endif
