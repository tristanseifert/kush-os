#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <getopt.h>

#include "version.h"
#include "InterfaceDescription.h"
#include "IDLParser.h"
#include "CodeGenerator.h"

/**
 * Global state for the compiler
 */
static struct {
    /// namespace to place stubs in
    std::string stubNs;
    /// directory to place output files in
    std::string outDir{"."};

    /// when set, we'll do a debug print of each interface loaded
    bool printInterfaces{false};

    /// filenames of input files
    std::vector<std::string> inFiles;
} gState;

/**
 * Parse the command line into the config state.
 *
 * @return 0 on success, 1 to exit with success, -1 for error exit.
 */
static int ParseCommandLine(int argc, char **argv) {
    int ch;

    // options for getopt
    static struct option options[] = {
        // allow specifying the namespace to place the RPC stubs in
        {"namespace", required_argument, nullptr, 'n'},
        // output directory for compiled files
        {"out", required_argument, nullptr, 'o'},
        // just print the read in interface, do not generate any code
        {"print", no_argument, nullptr, 'p'},
        // print the version and exit
        {"version", no_argument, nullptr, 'v'},
        {nullptr, 0, nullptr, 0}
    };

    // iterate until all are options read
    while((ch = getopt_long(argc, argv, "v", options, nullptr)) != -1) {

        switch(ch) {
            case 'v':
                std::cout << "This is idlc, the kush RPC IDL compiler (git rev "
                          << std::string(gVERSION_HASH).substr(0, 8) << ")" << std::endl;
                return 1;
                break;
            case 'n':
                gState.stubNs = std::string(optarg);
                break;
            case 'o':
                gState.outDir = std::string(optarg);
                break;
            case 'p':
                gState.printInterfaces = true;
                break;

            default:
                return -1;
        }
    }

    // read the file names
    for(size_t i = optind; i < argc; i++) {
        gState.inFiles.emplace_back(argv[i]);
    }

    if(gState.inFiles.empty()) {
        std::cerr << argv[0] << ": you must specify at least one input file" << std::endl;
        return -1;
    }
    return 0;
}

/**
 * Entry point for the IDL compiler. We expect one or more non-arguments passed in that are the
 * filenames of IDL files to generate code for, as well as optional switches that affect the code
 * generation process.
 */
int main(int argc, char **argv) {
    int ret = 0;

    // initialize
    if((ret = ParseCommandLine(argc, argv))) {
        return (ret == 1) ? 0 : -1;
    }

    std::filesystem::path outDir(gState.outDir);
    try {
        std::filesystem::create_directories(outDir);
    } catch(const std::filesystem::filesystem_error &e) {
        std::cerr << "Failed to create output directory '" << gState.outDir << "': " << e.what()
                  << std::endl;
        return -1;
    }

    // parse input files one by one
    IDLParser parser;
    std::vector<std::shared_ptr<InterfaceDescription>> interfaces;

    for(const auto &name : gState.inFiles) {
        try {
            std::vector<std::shared_ptr<InterfaceDescription>> tempIntfs;

            // open file and parse
            auto parsed = parser.parse(name, tempIntfs);
            if(!parsed) return -1;

            std::cout << "* Found " << tempIntfs.size() << " interface(s) in " << name
                      << std::endl;

            // print each interface for debugging if needed
            if(gState.printInterfaces) {
                for(const auto &intf : tempIntfs) {
                    std::cout << *intf << std::endl;
                }
            }

            // store the whole set of interfaces for later
            interfaces.insert(interfaces.end(), tempIntfs.begin(), tempIntfs.end());
        } catch(const std::exception &e) {
            std::cerr << "Failed to process '" << name << "': " << e.what() << std::endl;
            return -1;
        }
    }

    // generate the Cap'n proto and the C++ server and client stubs for each interface
    for(auto &intf : interfaces) {
        try {
            // set up the code generator
            std::cout << "* CodeGen for '" << intf->getName() << "' from " << intf->getSourceFilename() << std::endl;
            CodeGenerator gen(outDir, intf);

            // create the protocol files
            gen.generateSerialization();

            // and the server and client stubs
            gen.generateServerStub();
            gen.generateClientStub();
        } catch(const std::exception &e) {
            std::cerr << "Failed to process interface " << intf->getName() << ": " << e.what()
                      << std::endl;
            return -1;
        }
    }

    return ret;
}
