#ifndef CODEGENERATOR_CPPHELPERS_H
#define CODEGENERATOR_CPPHELPERS_H

#include <algorithm>
#include <cctype>
#include <string>

/**
 * Check if the given string is all caps.
 */
static inline bool IsAllCaps(const std::string &s) {
    return std::none_of(s.begin(), s.end(), ::islower);
}

/**
 * Convert lowerCamelCase and UpperCamelCase strings to UPPER_WITH_UNDERSCORES
 */
static inline std::string CamelToUpper(const std::string &camelCase) {
    std::string str(1, tolower(camelCase[0]));

    // First place underscores between contiguous lower and upper case letters.
    for (auto it = camelCase.begin() + 1; it != camelCase.end(); ++it) {
        if (isupper(*it) && *(it-1) != '_' && islower(*(it-1))) {
            str += "_";
        }

        str += *it;
    }

    // then to uppercase
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}


/**
 * Returns the name of the method for use in a stub class.
 */
static inline std::string GetMethodName(const InterfaceDescription::Method &m) {
    auto temp = m.getName();
    temp[0] = std::toupper(temp[0]);
    return temp;
}


/**
 * Returns the fully qualified name of the namespace in which a particular message struct was
 * encoded to.
 */
static inline std::string GetProtoMsgNsName(const InterfaceDescription::Method &m,
        const bool isResponse) {
    auto temp = m.getName();
    temp[0] = std::toupper(temp[0]);

    temp.append(isResponse ? "Response" : "Request");
    return std::string(CodeGenerator::kProtoNamespace) + "::" + temp;
}

/**
 * Returns the fully qualified name of the constant that contains the identifier of the given
 * method.
 */
static inline std::string GetMethodIdConst(const InterfaceDescription::Method &m) {
    auto temp = m.getName();
    temp = CamelToUpper(temp);
    return "rpc::_proto::messages::MESSAGE_ID_" + temp;
}

/**
 * Returns the name of the getter for the given argument.
 */
static inline std::string GetterNameFor(const InterfaceDescription::Argument &a) {
    auto temp = a.getName();
    temp[0] = std::toupper(temp[0]);
    return "get" + temp;
}
/**
 * Returns the name of the setter for the given argument.
 */
static inline std::string SetterNameFor(const InterfaceDescription::Argument &a) {
    auto temp = a.getName();
    temp[0] = std::toupper(temp[0]);
    return "set" + temp;
}


#endif
