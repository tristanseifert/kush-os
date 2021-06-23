#include "String.h"

#include "Log.h"

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



/**
 * Very quick and dirty UCS-2 to UTF-8 conversion.
 */
std::string util::ConvertUcs2ToUtf8(const std::span<uint16_t> &ucs2Str) {
    std::string str;

    for(const auto c : ucs2Str) {
        if(!c) break;

        if (c<0x80) str.push_back(c);
        else if (c<0x800) str.push_back(192+c/64), str.push_back(128+c%64);
        else if (c-0xd800u<0x800) goto error;
        else str.push_back(224+c/4096), str.push_back(128+c/64%64), str.push_back(128+c%64);
    }

    return str;

    // Invalid codepoint
error:;
    Warn("Invalid UCS-2 codepoint encountered");
    return "";
}
