#include "CodeGenerator.h"
#include "CodeGenerator+CppHelpers.h"
#include "InterfaceDescription.h"

#include <fstream>
#include <string>

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
                os << CppTypenameForArg(rets[0]) << ' ';
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
        os << CppTypenameForArg(a) << ' ';

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
std::string CodeGenerator::CppTypenameForArg(const Argument &a) {
    // look up built in types in the map
    if(a.isBuiltinType()) {
        // convert the argument to lowercase and lookup
        auto lowerName = a.getTypeName();
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        return gCppTypeNames.at(lowerName);
    }
    // otherwise, return its type as-is
    return a.getTypeName();
}

