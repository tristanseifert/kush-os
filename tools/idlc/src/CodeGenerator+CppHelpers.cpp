#include "CodeGenerator.h"
#include "CodeGenerator+CppHelpers.h"
#include "InterfaceDescription.h"

#include <fstream>
#include <string>
#include <unordered_map>

/**
 * Defines the mapping of lowercased IDL type name strings to C++ argument types.
 */
const std::unordered_map<std::string, std::string> CodeGenerator::gCppArgTypeNames{
    {"bool", "bool"},
    {"int8",  "int8_t"},  {"int16",  "int16_t"},  {"int32",  "int32_t"},  {"int64",  "int64_t"},
    {"uint8", "uint8_t"}, {"uint16", "uint16_t"}, {"uint32", "uint32_t"}, {"uint64", "uint64_t"},
    {"float32", "float"}, {"float64", "double"},
    {"string", "std::string"}, {"blob", "std::span<std::byte>"},

    {"void", "Void"},
};
/// Defines the mapping of lowercased IDL type names to C++ return value type names
const std::unordered_map<std::string, std::string> CodeGenerator::gCppReturnTypeNames{
    {"bool", "bool"},
    {"int8",  "int8_t"},  {"int16",  "int16_t"},  {"int32",  "int32_t"},  {"int64",  "int64_t"},
    {"uint8", "uint8_t"}, {"uint16", "uint16_t"}, {"uint32", "uint32_t"}, {"uint64", "uint64_t"},
    {"float32", "float"}, {"float64", "double"},
    {"string", "std::string"}, {"blob", "std::vector<std::byte>"},

    {"void", "Void"},
};

/**
 * Writes the server method definition for the given method.
 */
void CodeGenerator::cppWriteMethodDef(std::ofstream &os, const Method &m,
        const std::string &namePrefix) {
    // return type
    if(m.isAsync()) {
        os << "void ";
    } else {
        auto &rets = m.getReturns();
        if(rets.empty()) {
            os << "void ";
        } else {
            // single argument?
            if(rets.size() == 1) {
                os << CppTypenameForArg(rets[0], false) << ' ';
            }
            // more than one; we have to define a struct type
            else {
                throw std::runtime_error("multiple return types not yet implemented");
            }
        }
    }

    // its name and opening bracket
    const auto methodName = GetMethodName(m);
    os << namePrefix << methodName << '(';

    // any arguments
    const auto &params = m.getParameters();
    for(size_t i = 0; i < params.size(); i++) {
        const auto &a = params[i];

        // print its type (it's a const reference if it's non primitive)
        if(!a.isPrimitiveType()) {
            os << "const ";
        }
        os << CppTypenameForArg(a, true) << ' ';

        if(!a.isPrimitiveType()) {
            os << '&';
        }

        // its name and a comma if not last
        os << a.getName();

        if(i != params.size()-1) {
            os << ", ";
        }
    }

    // closing bracket
    os << ')';
}

/**
 * Returns the C++ type name for the given argument.
 */
std::string CodeGenerator::CppTypenameForArg(const Argument &a, const bool isArg) {
    // look up built in types in the map
    if(a.isBuiltinType()) {
        // convert the argument to lowercase and lookup
        auto lowerName = a.getTypeName();
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        return (isArg ? gCppArgTypeNames : gCppReturnTypeNames).at(lowerName);
    }
    // otherwise, return its type as-is
    return a.getTypeName();
}

