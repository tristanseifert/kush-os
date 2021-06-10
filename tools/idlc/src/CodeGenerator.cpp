#include "CodeGenerator.h"
#include "InterfaceDescription.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

/**
 * Defines the mapping of lowercased type name strings to Cap'n Proto types.
 */
const std::unordered_map<std::string, std::string> CodeGenerator::gProtoTypeNames{
    {"bool", "Bool"},
    {"int8",  "Int8"},  {"int16",  "Int16"},  {"int32",  "Int32"},  {"int64",  "Int64"},
    {"uint8", "UInt8"}, {"uint16", "UInt16"}, {"uint32", "UInt32"}, {"uint64", "UInt64"},
    {"float32", "Float32"}, {"float64", "Float64"},
    {"string", "Text"}, {"blob", "Data"},

    {"void", "Void"},
};


/**
 * Returns the name of the given interface converted to the name of the request or response Cap'n
 * Proto structure.
 */
static inline std::string GetMessageStructName(const std::string &name, const bool isResponse) {
    auto temp = name;
    temp[0] = std::toupper(temp[0]);
    return temp + (isResponse ? "Response" : "Request");
}
/**
 * Returns the name of the given interface's identifier constant.
 */
static inline std::string GetMessageIdConstName(const std::string &name) {
    auto temp = name;
    temp[0] = std::toupper(temp[0]);
    return "messageId" + temp;
}
/**
 * Returns the name of a message argument converted to what's suitable as a field name in Cap'n
 * Proto messages.
 */
static inline std::string GetMessageArgName(const std::string &name) {
    auto temp = name;
    temp[0] = std::tolower(temp[0]);
    return temp;
}



/**
 * Initializes the code generator.
 */
CodeGenerator::CodeGenerator(const std::filesystem::path &_outDir, const IDPointer &_interface) :
    interface(_interface), outDir(_outDir) {
    // nothing
}

/**
 * Generates the Cap'n Proto file for the interface.
 */
void CodeGenerator::generateProto() {
    // open the output stream
    auto fileName = this->outDir / (this->interface->getName() + ".capnp");
    std::cout << "    * Wire format: " << fileName.string() << std::endl;

    std::ofstream os(fileName.string(), std::ofstream::trunc);

    // generate file header and place in private namespace
    os << "# This is an automatically generated file (by idlc). Do not edit." << std::endl;
    os << "# Generated from " << this->interface->getSourceFilename() << " for interface "
       << this->interface->getName() << std::endl;
    os << "@0x" << std::hex << this->interface->getIdentifier() << ';' << std::endl;

    os << R"(using Cxx = import "/capnp/c++.capnp";)" << std::endl;
    os << "$Cxx.namespace(\"" << kProtoNamespace << "\");" << std::endl;

    // output info for each method
    os << R"(
######################
# Method defenitions #
######################)" << "\n\n";

    for(const auto &method : this->interface->getMethods()) {
        this->protoWriteMethod(os, method);
    }
}

/**
 * Writes the structure info for the given method.
 */
void CodeGenerator::protoWriteMethod(std::ofstream &os, const InterfaceDescription::Method &m) {
    os << R"(############################################################
# Structures for message ')" << m.getName() << "'" << std::endl;

    // define the message id
    os << "const " << GetMessageIdConstName(m.getName()) << ":UInt64 = 0x" << std::hex
       << m.getIdentifier() << ';' << std::endl << std::endl;

    // start with the request structure
    os << "struct " << GetMessageStructName(m.getName(), false) << " {" << std::endl;
    this->protoWriteArgs(os, m.getParameters());
    os << "}" << std::endl;

    // asynchronous messages do not have a reply structure
    if(m.isAsync()) {
        os << "# Message is async, no response struct needed" << std::endl;
    } 
    // if message is synchronous, define its reply structure
    else {
        os << "struct " << GetMessageStructName(m.getName(), true) << " {" << std::endl;
        this->protoWriteArgs(os, m.getReturns());
        os << "}" << std::endl;
    }

    os << std::endl;
}

/**
 * Writes the argument fields provided out to the struct sequentially.
 */
void CodeGenerator::protoWriteArgs(std::ofstream &os,
        const std::vector<InterfaceDescription::Argument> &args) {
    for(size_t i = 0; i < args.size(); i++) {
        // write out the name label
        const auto &a = args.at(i);

        if(!a.isPrimitiveType()) {
            os << "# Custom serialization type; was '" << a.getTypeName() << '\'' << std::endl;
        }

        os << "    " << std::setw(28) << GetMessageArgName(a.getName()) << " @" << i << ": ";

        // write its type
        if(a.isPrimitiveType()) {
            // convert from serialization names to the Cap'n Proto names
            auto lowerName = a.getTypeName();
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                    [](unsigned char c){ return std::tolower(c); });

            os << gProtoTypeNames.at(lowerName);
        } else {
            // non-primitive types employ manual serialization to blobs
            os << "Data";
        }

        os << ';' << std::endl;
    }
}
