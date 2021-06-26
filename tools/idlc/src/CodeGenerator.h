#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "InterfaceDescription.h"

/**
 * Encapsulates the code generation for the Cap'n Proto structs used as part of the wire format of
 * the messages, as well as the C++ server and client stubs.
 *
 * For each interface, you will create an instance of the code generator.
 */
class CodeGenerator {
    using IDPointer = std::shared_ptr<InterfaceDescription>;
    using Argument = InterfaceDescription::Argument;
    using Method = InterfaceDescription::Method;

    public:
        /// namespace in which all protocol definitions live
        constexpr static const std::string_view kProtoNamespace{"rpc::_proto::messages"};

    public:
        CodeGenerator(const std::filesystem::path &outDir, const IDPointer &interface);

        /// Generates the Cap'n Proto messages for each method's params and reply.
        void generateProto();
        /// Generates the server stub for the interface
        void generateServerStub();
        /// Generates the client stub for the interface
        void generateClientStub();

    private:
        void protoWriteMethod(std::ofstream &, const Method &);
        void protoWriteArgs(std::ofstream &, const std::vector<Argument> &);

        static std::string ProtoTypenameForArg(const Argument &);

        void serverWriteInfoBlock(std::ofstream &);
        void serverWriteHeader(std::ofstream &);

        void serverWriteImpl(std::ofstream &);
        void serverWriteMarshallMethod(std::ofstream &, const Method &);
        void serverWriteMarshallMethodReply(std::ofstream &, const Method &);

        void clientWriteInfoBlock(std::ofstream &);
        void clientWriteHeader(std::ofstream &);

        void clientWriteImpl(std::ofstream &);
        void clientWriteMarshallMethod(std::ofstream &, const Method &);
        void clientWriteMarshallMethodReply(std::ofstream &, const Method &);

        void cppWriteStructs(std::ofstream &);
        void serWriteMethod(std::ofstream &, const Method &);
        void serWriteArgs(std::ofstream &, const std::vector<Argument> &);
        void serWriteSerializers(std::ofstream &, const std::vector<Argument> &, const std::string &);

        void cppWriteMethodDef(std::ofstream &, const Method &, const std::string &prefix = "", const std::string &classPrefix = "");
        void cppWriteReturnStruct(std::ofstream &, const Method &);
        void cppWriteIncludes(std::ofstream &);
        void cppWriteCustomTypeHelpers(std::ofstream &);
        static std::string CppTypenameForArg(const Argument &, const bool isArg);

    private:
        /// mapping of IDL types to wire format sizes
        static const std::unordered_map<std::string, size_t> gWireSizes;
        /// Whether a particular IDL type is encoded as a blob
        static const std::unordered_map<std::string, bool> gWireIsBlob;

        // mapping of the type names defined in the IDL to Cap'n Proto names
        static const std::unordered_map<std::string, std::string> gProtoTypeNames;
        // mapping of the type names defined in the IDL to C++ type names
        static const std::unordered_map<std::string, std::string> gCppArgTypeNames;
        static const std::unordered_map<std::string, std::string> gCppReturnTypeNames;

        // timestamp for generation (ins ISO 8601 format)
        std::string creationTimestamp;

        // this is the interface for which we're generating code
        IDPointer interface;
        // directory into which output files are written
        std::filesystem::path outDir;

        // filename for the Cap'n Proto file
        std::filesystem::path protoFileName;
};

#endif
