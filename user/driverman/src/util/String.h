/*
 * Various string utilities
 */
#ifndef UTIL_STRING_H
#define UTIL_STRING_H

// uncomment to use locale functions instead of the C library locale
// #define UTIL_USE_LOCALE

#ifdef UTIL_USE_LOCALE
#include <locale>
#endif

#include <cctype>
#include <string>

namespace util {
/**
 * Trim any leading space from the given string.
 */
inline std::string & ltrim(std::string & str) {
    auto it2 = std::find_if(str.begin(), str.end(), [](char ch) -> bool {
#ifdef UTIL_USE_LOCALE
        return !std::isspace<char>(ch, std::locale::classic());
#else
        return ::isspace(ch);
#endif
    });
    str.erase(str.begin(), it2);
    return str;
}

/**
 * Trim any trailing space from the given string.
 */
inline std::string & rtrim(std::string & str) {
    auto it1 =  std::find_if(str.rbegin(), str.rend(), [](char ch) -> bool {
#ifdef UTIL_USE_LOCALE
        return !std::isspace<char>(ch, std::locale::classic());
#else
        return ::isspace(ch);
#endif
    });
    str.erase(it1.base(), str.end());
    return str;
}

/**
 * Trim whitespace from both ends of the string.
 */
inline std::string & trim(std::string &str) {
    return ltrim(rtrim(str));
}

/**
 * Trims whitespace from both ends of the given string view.
 */
inline std::string trim(const std::string_view &sv) {
    std::string str(sv);
    return trim(str);
}

} // namespace util

#endif
