#ifndef ROOTSRV_STRINGHELPERS_H
#define ROOTSRV_STRINGHELPERS_H

#include <string>
#include <vector>

static inline std::string & ltrim(std::string & str) {
    auto it2 =  std::find_if(str.begin(), str.end(), [](char ch){ return !std::isspace(ch); });
    str.erase(str.begin(), it2);
    return str;
}
static inline std::string & rtrim(std::string & str) {
    auto it1 =  std::find_if(str.rbegin(), str.rend(), [](char ch){ return !std::isspace(ch); });
    str.erase(it1.base(), str.end());
    return str;
}
static inline std::string & trim(std::string & str) {
    return ltrim(rtrim(str));
}

/**
 * Splits a space separated string into a vector. Arguments containing spaces may be quoted.
 */
static inline bool SplitStringArgs(const std::string_view &command, std::vector<std::string> &qargs){
    int len = command.length();
    bool qot = false, sqot = false;
    int arglen;
    for(int i = 0; i < len; i++) {
        int start = i;
        if(command[i] == '\"') {
            qot = true;
        }
        else if(command[i] == '\'') sqot = true;

        if(qot) {
            i++;
            start++;
            while(i<len && command[i] != '\"')
                    i++;
            if(i<len)
                    qot = false;
            arglen = i-start;
            i++;
        }
        else if(sqot) {
            i++;
            while(i<len && command[i] != '\'')
                    i++;
            if(i<len)
                    sqot = false;
            arglen = i-start;
            i++;
        }
        else{
            while(i<len && command[i]!=' ')
                    i++;
            arglen = i-start;
        }

        qargs.push_back(std::string(command.substr(start, arglen)));
    }
    if(qot || sqot) return false;
    return true;
}

#endif
