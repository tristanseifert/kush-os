#include "Console.h"

#include "Boot/Helpers.h"
#include "Util/String.h"

using namespace Platform::Amd64Uefi;

struct stivale2_struct_tag_terminal *Console::gTerminal{nullptr};
Console::TerminalWrite Console::gTerminalWrite{nullptr};
uint16_t Console::gDebugconPort{0};

/**
 * Initialize the platform console.
 *
 * This is basically a multiplexer between the stivale2 terminal, an IO port console and serial
 * port.
 *
 * @param info Environment from the bootloader
 */
void Console::Init(struct stivale2_struct *info) {
    // check for the terminal info tag
    gTerminal = reinterpret_cast<struct stivale2_struct_tag_terminal *>
        (Stivale2::GetTag(info, STIVALE2_STRUCT_TAG_TERMINAL_ID));
    if(gTerminal) {
        gTerminalWrite = reinterpret_cast<TerminalWrite>(gTerminal->term_write);
    }

    // get at the command line (to determine the serial/debugcon)
    auto cmd = reinterpret_cast<struct stivale2_struct_tag_cmdline *>
        (Stivale2::GetTag(info, STIVALE2_STRUCT_TAG_CMDLINE_ID));
    if(cmd) {
        auto cmdline = reinterpret_cast<const char *>(cmd->cmdline);
        ParseCmd(cmdline);
    }
}

const char *equals = " = ";
const char *nl = "\n";

/**
 * Parse the command line string specified to find all specified output devices.
 *
 * These are specified by the `-console` argument.
 */
void Console::ParseCmd(const char *cmdline) {
    char ch;

    // the key currently being read
    const char *key;
    size_t keyLen;
    // the avlue currently being read
    const char *value;
    size_t valueLen;

    enum class State {
        // not reading anything
        IDLE,
        // reading the key
        KEY,
        // reading the value
        VALUE,
    } state = State::IDLE;

    // loop through the command line
    do {
        ch = *cmdline;

        switch(state) {
            // we can start reading a key
            case State::IDLE:
                switch(ch) {
                    // keys start with a dash
                    case '-':
                        state = State::KEY;
                        key = cmdline+1;
                        keyLen = 0;
                        break;
                    // otherwise, ignore it
                    default:
                        break;
                }
                break;

            // key is being read
            case State::KEY:
                switch(ch) {
                    // end of key
                    case '=':
                        state = State::VALUE;
                        value = cmdline+1;
                        valueLen = 0;
                        break;
                    // space terminates (we only care about keys with associated values)
                    case ' ':
                        state = State::IDLE;
                        break;
                    // part of the key
                    default:
                        keyLen++;
                        break;
                }
                break;

            // value is being read
            case State::VALUE:
                switch(ch) {
                    // end of string or end of token
                    case '\0':
                    case ' ':
                        // key should be "console"; if so, parse it further
                        if(!Util::strncmp("console", key, keyLen)) {
                            ParseCmdToken(value, valueLen);
                        }

                        // reset state for next token
                        state = State::IDLE;
                        break;
                    // part of the value
                    default:
                        valueLen++;
                        break;
                }
                break;
        }

        // go to next character
        cmdline++;
    } while(ch);
}

/**
 * Parse the value for a `console` parameter in the command line.
 *
 * The value consists of comma-separated values, the first of which indicates the type of the
 * output. The following types are supported:
 *
 * - `serial`: An 16550-type UART. First parameter is the IO port base, then the baud rate. The
 *   port will be configured as 8N1.
 * - `debugcon`: Write characters to the specified IO port.
 *
 * @param value Start of the value of this command token
 * @param valueLen Number of characters in the command token
 */
void Console::ParseCmdToken(const char *value, const size_t valueLen) {
    const char *type = value;
    size_t typeLen{0};

    // extract the type
    for(size_t i = 0; i < valueLen; i++) {
        auto ch = *value++;
        if(ch == ',') break;
        typeLen++;
    }

    // debugcon?
    if(!Util::strncmp("debugcon", type, typeLen)) {
        gDebugconPort = Util::strtol(value, nullptr, 0);
    }
    // unknown type :(
    else {
        // TODO: display error? what can we do at this point
        Write(type, typeLen);
    }
}

/**
 * Print a message to the console.
 *
 * @param string A character string to output to the console
 * @param numChars Number of characters to read from `string`
 */
void Console::Write(const char *string, const size_t numChars) {
    // output to all character based outputs
    if(gDebugconPort) {
        for(size_t i = 0; i < numChars; i++) {
            const auto ch = string[i];
            if(gDebugconPort) asm volatile("outb %0,%1":: "a"(ch), "Nd"(gDebugconPort) : "memory");
        }
    }

    // lastly, print to the loader console
    if(gTerminalWrite) {
        gTerminalWrite(string, numChars);
    }
}
