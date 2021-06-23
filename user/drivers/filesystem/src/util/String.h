#pragma once

#include <cstdint>
#include <span>
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

/**
 * Converts an UCS-2 string to an UTF-8 encoded C++ string.
 */
std::string ConvertUcs2ToUtf8(const std::span<uint16_t> &ucs2Str);
}

