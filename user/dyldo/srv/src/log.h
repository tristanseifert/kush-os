#ifndef LOG_H
#define LOG_H

#include <cstdio>
#include <utility>

#include <fmt/format.h>

template <typename ... Args>
void L(Args&& ... args) {
    const auto str = fmt::format(std::forward<Args>(args)...);
    fprintf(stderr, "%s\n", str.c_str());
}

#endif
