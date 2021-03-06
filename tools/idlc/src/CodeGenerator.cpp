#include "CodeGenerator.h"

#include <cstdio>
#include <ctime>
#include <string>
#include <unordered_map>

/**
 * Defines the mapping of lowercased IDL type name strings to Cap'n Proto types.
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
 * Initializes the code generator.
 */
CodeGenerator::CodeGenerator(const std::filesystem::path &_outDir, const IDPointer &_interface) :
    interface(_interface), outDir(_outDir) {
    // get the generation timestamp
    time_t rawtime;
    struct tm *timeinfo;
    constexpr static const size_t kBufferSz{60};
    char buffer[kBufferSz] {};

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, kBufferSz, "%Y-%m-%dT%H:%M:%S%z", timeinfo);

    this->creationTimestamp = std::string(buffer);
}
