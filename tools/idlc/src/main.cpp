#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <getopt.h>

#include "InterfaceDescription.h"
#include "IDLParser.h"

/**
 * Global state for the compiler
 */
static struct {
    /// namespace to place stubs in
    std::string stubNs;
    /// directory to place output files in
    std::string outDir{"."};

    /// filenames of input files
    std::vector<std::string> inFiles;
} gState;

/**
 * Parse the command line into the config state.
 */
static int ParseCommandLine(int argc, char **argv) {
    int ch;

    // options for getopt
    static struct option options[] = {
        // allow specifying the namespace to place the RPC stubs in
        {"namespace", required_argument, nullptr, 'n'},
        // output directory for compiled files
        {"out", required_argument, nullptr, 'o'},
        {nullptr, 0, nullptr, 0}
    };

    // iterate until all are options read
    while((ch = getopt_long(argc, argv, "", options, nullptr)) != -1) {

        switch(ch) {
            case 'n':
                gState.stubNs = std::string(optarg);
                break;
            case 'o':
                gState.outDir = std::string(optarg);
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
    int ret = 1;

    // initialize
    if(ParseCommandLine(argc, argv)) {
        return -1;
    }

    // parse input files one by one
    IDLParser parser;
    std::vector<std::shared_ptr<InterfaceDescription>> interfaces;

    for(const auto &name : gState.inFiles) {
        try {
            // open file and parse
            auto parsed = parser.parse(name);
            if(!parsed) return -1;

            // store it
            interfaces.emplace_back(std::move(parsed));
        } catch(const std::exception &e) {
            std::cerr << "failed to process '" << name << "': " << e.what() << std::endl;
            return -1;
        }
    }

    return ret;
}
