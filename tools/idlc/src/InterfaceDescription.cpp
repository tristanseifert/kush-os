#include "InterfaceDescription.h"

#include "util/MurmurHash2.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string>

using Argument = InterfaceDescription::Argument;
using Method = InterfaceDescription::Method;

/**
 * List of built in type names. These are matched against in a case insensitive manner.
 */
const std::array<std::string, 13> Argument::gPrimitiveTypeNames {
    "bool", 
    "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",
    "float32", "float64",
    "string", "blob"
};

/**
 * Creates a new argument with the given name and type name. We'll determine at this point if the
 * type is one of the built-in primitive types, or if custom serialization is required.
 */
Argument::Argument(const std::string &_name, const std::string &_typeName) :
    name(_name), typeName(_typeName) {
    // check if it's a primitive
    auto lowerName = _typeName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](unsigned char c){ return std::tolower(c); });

    this->isBuiltin = std::find(gPrimitiveTypeNames.begin(), gPrimitiveTypeNames.end(),
            lowerName) != gPrimitiveTypeNames.end();
    if(this->isBuiltin) {
        this->isPrimitive = (lowerName != "string") && (lowerName != "blob");
    }
}

/**
 * Prints a textual representation of a method argument.
 */
std::ostream& operator<<(std::ostream& os, const InterfaceDescription::Argument& m) {
    if(m.isBuiltin) {
        os << '(' << m.name <<  ": " << m.typeName << ')';
    } else {
        os << '[' << m.name <<  ": " << m.typeName << ']';
    }

    return os;
}



/**
 * Initializes a new method; if the identifier is not provided, it's automatically generated from
 * the name by hashing it.
 */
Method::Method(const std::string &_name, const uint64_t identifier) : name(_name) {
    if(!identifier) {
        this->identifier = MurmurHash64A(this->name.data(), this->name.size(), kMethodNameHashSeed);
    }
    // use the pre-provided identifier instead
    else {
        this->identifier = identifier;
    }
}

/**
 * Prints a textual representation of a method.
 */
std::ostream& operator<<(std::ostream& os, const InterfaceDescription::Method& m) {
    using namespace std;

    os << setw(32) << m.name << " $" << std::hex << setw(16) << m.identifier << " (" << (m.async ? "A" : "S") << ')' << std::endl;

    if(!m.params.empty()) {
        os << setw(32) << "Inputs:" << ' ';

        for(const auto &arg : m.params) {
            os << arg << ' ';
        }

        os << std::endl;
    } else {
        os << setw(32) << "Inputs:" << " None" << std::endl;
    }

    if(!m.returns.empty() && !m.async) {
        os << setw(32) << "Returns:" << ' ';

        for(const auto &arg : m.returns) {
            os << arg << ' ';
        }

        os << std::endl;
    } else {
        os << setw(32) << "Returns:" << " None" << std::endl;
    }

    return os;
}



/**
 * Creates a new interface description.
 */
InterfaceDescription::InterfaceDescription(const std::string &_name,
        const std::string &_filename) : filename(_filename), name(_name) {
    this->identifier = MurmurHash64A(this->name.data(), this->name.size(), kInterfaceNameHashSeed);
}

/**
 * Prints a textual representation of the interface description.
 */
std::ostream& operator<<(std::ostream& os, const InterfaceDescription& i) {
    os << "Interface '" << i.name << "' ($" << std::setw(16) << std::hex << i.identifier
       << ") has " << i.methods.size() << " method(s):" << std::endl;

    for(const auto &m : i.methods) {
        // print each method to a buffer so we can indent it
        std::stringstream str;
        str << m;

        // indent each line and print it
        os << str.str();
    }

    return os;
}
