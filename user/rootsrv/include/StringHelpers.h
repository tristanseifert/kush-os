#ifndef ROOTSRV_STRINGHELPERS_H
#define ROOTSRV_STRINGHELPERS_H

#include <string>

std::string & ltrim(std::string & str) {
    auto it2 =  std::find_if(str.begin(), str.end(), [](char ch){ return !std::isspace(ch); });
    str.erase(str.begin(), it2);
    return str;
}
std::string & rtrim(std::string & str) {
    auto it1 =  std::find_if(str.rbegin(), str.rend(), [](char ch){ return !std::isspace(ch); });
    str.erase(it1.base(), str.end());
    return str;
}
std::string & trim(std::string & str) {
    return ltrim(rtrim(str));
}

#endif
