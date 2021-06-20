#include "String.h"

#include <algorithm>

/**
 * Converts a string by swapping pairs of adjacent characters. If the string is not an even number
 * of characters, the last character is left alone.
 */
void util::ConvertAtaString(std::string &str) {
    const auto numPairs = str.length() / 2;
    for(size_t i = 0; i < numPairs; i++) {
        std::swap(str[(i * 2)], str[(i * 2) + 1]);
    }
}

/**
 * Trims all trailing whitespace from the string.
 */
void util::TrimTrailingWhitespace(std::string &str) {
    if(str.empty()) return;

    auto nonSpace = str.find_last_not_of(' ');
    if(nonSpace != std::string::npos && nonSpace < str.length()-1) {
        str.erase(nonSpace+1);
    }
}

