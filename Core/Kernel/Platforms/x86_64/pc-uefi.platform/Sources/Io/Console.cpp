#include "Console.h"

#include "Boot/Helpers.h"
#include "Util/String.h"

#include <FbCons/Console.h>
#include <Logging/Console.h>
#include <Vm/Map.h>
#include <Vm/ContiguousPhysRegion.h>

#include <new>

using namespace Platform::Amd64Uefi;

struct stivale2_struct_tag_terminal *Console::gTerminal{nullptr};
Console::TerminalWrite Console::gTerminalWrite{nullptr};
Kernel::Vm::MapEntry *Console::gFb{nullptr};
void *Console::gFbBase{nullptr};
size_t Console::gFbWidth{0}, Console::gFbHeight{0}, Console::gFbStride{0};
Platform::Shared::FbCons::Console *Console::gFbCons{nullptr};
uint16_t Console::gDebugconPort{0};

/**
 * @brief Initialize the platform console.
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

/**
 * @brief Parse the command line string specified to find all specified output devices.
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
 * @brief Parse the value for a `console` parameter in the command line.
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
    }
}

/**
 * @brief Print a message to the console.
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
    if(gFbCons) {
        gFbCons->write(string, numChars);
    }
}

/**
 * @brief Prepares the console for virtual memory mode.
 *
 * This disables the bootloader console, since we'll no longer have its code mapped.
 */
void Console::PrepareForVm(struct stivale2_struct *info, Kernel::Vm::Map *map) {
    // disable bootloader console
    Console::Write("Preparing console for VM enablement...\n", 39);
    gTerminalWrite = nullptr;
}

/**
 * @brief Prepare for using bitmap console
 *
 * This fetches more framebuffer info, then stores it for later, so that when the virtual map is
 * actually enabled, we can just enable the console.
 *
 * @param info Bootloader information structure
 * @param fb Framebuffer VM object
 * @param base Base address of framebuffer, as mapped in memory
 *
 * @return 0 on success
 */
int Console::SetFramebuffer(struct stivale2_struct *info, Kernel::Vm::MapEntry *fb, void *base) {
    // specify a `nullptr` framebuffer to clear its state
    if(!fb) {
        //if(gFb) gFb->release();
        gFb = nullptr;

        return 0;
    }

    // get framebuffer info
    REQUIRE(info, "invalid loader info");
    REQUIRE(fb && base, "invalid framebuffer base");

    auto fbInfo = reinterpret_cast<const stivale2_struct_tag_framebuffer *>
        (Stivale2::GetTag(info, STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID));
    if(!fbInfo) {
        return -1;
    }

    // store it for later
    gFbWidth = fbInfo->framebuffer_width;
    gFbHeight = fbInfo->framebuffer_height;
    gFbStride = fbInfo->framebuffer_pitch;

    gFb = fb;
    gFbBase = base;

    return 0;
}

/**
 * @brief Initialize framebuffer console
 *
 * This is called once the kernel VM map is activated, and we can set up the framebuffer.
 */
void Console::VmEnabled() {
    using FbCons = Platform::Shared::FbCons::Console;

    // bail if no framebuffer was mapped
    if(!gFb) return;

    // create it out of the static storage
    static KUSH_ALIGNED(64) uint8_t gBuf[sizeof(FbCons)];
    auto ptr = reinterpret_cast<FbCons *>(gBuf);

    new(ptr) FbCons(reinterpret_cast<uint32_t *>(gFbBase), FbCons::ColorOrder::ARGB, 1024, 768);

    gFbCons = ptr;
}
