#include "CodeGenerator.h"
#include "InterfaceDescription.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>


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
 * Generates the Cap'n Proto file for the interface.
 */
void CodeGenerator::generateProto() {
    // open the output stream
    this->protoFileName = this->outDir / (this->interface->getName() + ".capnp");
    std::cout << "    * Wire format: " << this->protoFileName.string() << std::endl;

    std::ofstream os(this->protoFileName.string(), std::ofstream::trunc);

    // generate file header and place in private namespace
    os << "# This is an automatically generated file (by idlc). Do not edit." << std::endl;
    os << "# Generated from " << this->interface->getSourceFilename() << " for interface "
       << this->interface->getName() << " at " << this->creationTimestamp << std::endl;
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
void CodeGenerator::protoWriteMethod(std::ofstream &os, const Method &m) {
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
void CodeGenerator::protoWriteArgs(std::ofstream &os, const std::vector<Argument> &args) {
    for(size_t i = 0; i < args.size(); i++) {
        // write out the name label
        const auto &a = args.at(i);

        if(!a.isBuiltinType()) {
            os << "# Custom serialization type; was '" << a.getTypeName() << '\'' << std::endl;
        }

        os << "    " << std::setw(28) << GetMessageArgName(a.getName()) << " @" << i << ": ";

        // write its type and finish the line
        os << ProtoTypenameForArg(a) << ';' << std::endl;
    }
}

/**
 * Convert an IDL type to a Cap'n Proto type name.
 */
std::string CodeGenerator::ProtoTypenameForArg(const Argument &a) {
    if(a.isBuiltinType()) {
        // convert from serialization names to the Cap'n Proto names
        auto lowerName = a.getTypeName();
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        return gProtoTypeNames.at(lowerName);
    }
    // non-primitive types employ manual serialization to blobs
    return "Data";
}

