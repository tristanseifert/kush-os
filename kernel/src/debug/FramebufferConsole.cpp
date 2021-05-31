#include "FramebufferConsole.h"
#include "BitmapFonts.h"

#include <log.h>
#include <string.h>

using namespace debug;

/// determine whether the given char is an ASCII digit
constexpr static inline bool IsDigit(const char ch) {
    return (ch >= '0') && (ch <= '9');
}
/// convert ASCII string to number
constexpr static unsigned int StrToInt(const char *str) {
  unsigned int i = 0U;
  while (IsDigit(*str)) {
    i = i * 10U + (unsigned int)((*str++) - '0');
  }
  return i;
}

/**
 * Initializes the framebuffer console.
 *
 * We'll fill the entire screen with the default background color as part of the initialization.
 */
FramebufferConsole::FramebufferConsole(uint32_t *fb, const ColorOrder format, const size_t w,
        const size_t h, const size_t stride) : buffer(fb), bufferFormat(format) {
    // calculate stride if needed
    this->bufferSize[Size::Width] = w;
    this->bufferSize[Size::Height] = h;

    if(!stride) {
        this->bufferStride = w * sizeof(uint32_t);
    } else {
        this->bufferStride = stride;
    }

    // configure some drawing defaults
    this->font = &BitmapFontHelper::gFonts[0];

    this->bufferChars[Size::Width] = w / font->width;
    this->bufferChars[Size::Height] = h / font->height;

    // clear screen
    this->updateColors();
    this->clear();
}

/**
 * Prints each character in the given string.
 */
void FramebufferConsole::write(const char *str, const size_t _length) {
    auto strLen = _length;
    if(!strLen) {
        auto temp = str;
        while(*temp++) strLen++;
    }

    for(size_t i = 0; i < strLen; i++) {
        this->write(*str++);
    }
}

/**
 * Prints a character to the screen.
 *
 * This is implemented as a small state machine so we can properly handle the ANSI escape
 * sequences.
 */
void FramebufferConsole::write(const char ch) {
    switch(this->writeState) {
        /**
         * Interpret the character and determine whether it's the start of an escape sequence, or
         * just a printable character.
         */
        case WriteState::Idle:
            if(ch != '\033') [[likely]] {
                this->print(ch);
            } else {
                this->writeState = WriteState::AnsiEscapeStart;
            }
            break;

        /**
         * We've received an escape sequence's first byte, so inspect this character to figure
         * out what kind of escape sequence we've got.
         */
        case WriteState::AnsiEscapeStart:
            if(ch == '[') {
                this->writeState = WriteState::AnsiReadingCSI;

                this->ansiBufUsed = 0;
                memset(this->ansiBuf, 0, kAnsiBufSize);
            }
            // invalid/unsupported escape sequence
            else {
                this->writeState = WriteState::Idle;
            }
            break;

        /**
         * The escape sequence is a CSI. These start off by zero or more parameter bytes (in the
         * range of 0x30-0x3F), then by any number of intermediate bytes (0x20-0x2F) and finally
         * the final byte (0x40-0x7E) that actually determines what escape sequence is to be
         * invoked.
         */
        case WriteState::AnsiReadingCSI:
            // write it into the escape sequence buffer
            this->ansiBuf[this->ansiBufUsed++] = ch;

            // is this the end of the sequence?
            if(ch >= 0x40 && ch <= 0x7E) {
                this->processAnsi();
                this->writeState = WriteState::Idle;
            }
            // otherwise, if the buffer is full, reset to regular printing
            else if(this->ansiBufUsed == kAnsiBufSize) {
                this->writeState = WriteState::Idle;
            }

            break;
    }
}

/**
 * Processes the ANSI escape sequence that is currently buffered.
 */
void FramebufferConsole::processAnsi() {
    // ignore empty sequences
    if(!this->ansiBufUsed) return;


    // figure out the type of escape sequence (by the last character)
    const auto finalByte = this->ansiBuf[this->ansiBufUsed - 1];
    switch(finalByte) {
        // SGR (Select Graphic Rendition)
        case 'm':
            this->processAnsiSgr();
            break;

        // CUP (Set cursor position)
        case 'H':
            this->processAnsiCup();
            break;

        // unknown escape sequence type
        default:
            log("Unhandled ANSI sequence: '%s' (%lu chars, final $%02x)", this->ansiBuf,
                    this->ansiBufUsed, finalByte);
            break;
    }
}

/**
 * Processes a Select Graphic Rendition escape sequence. Currently, only the reset and foreground
 * and background colors are implemented.
 */
void FramebufferConsole::processAnsiSgr() {
    size_t numberRead = 0;
    char number[8]{0};
    auto readPtr = this->ansiBuf;

    // if a single character was read, it was a reset
    if(this->ansiBufUsed == 1) {
        this->processAnsiSgr(this->ansiBuf, 0);
        goto beach;
    }

    /**
     * Scan the string for numbers, seperated by semicolons. If there is no number specified
     * between a semicolon, 0 is assumed.
     *
     * To make this easier, we overwrite the last character (the 'm') with a NUL character.
     */
    this->ansiBuf[this->ansiBufUsed - 1] = '\0';

    while(const auto ch = *readPtr++) {
        // semicolon terminates sequence
        if(ch == ';') {
            this->processAnsiSgr(number, numberRead);

            memset(number, 0, sizeof(number));
            numberRead = 0;
        }
        // copy the string otherwise
        else {
            number[numberRead++] = ch;
        }
    }

    // handle any remaining numbers
    if(numberRead) {
        this->processAnsiSgr(number, numberRead);
    }

beach:;
    // ensure the color palettes are in sync
    this->updateColors();
}

/**
 * Process a single SGR attribute
 */
void FramebufferConsole::processAnsiSgr(const char *str, const size_t strLen) {
    // treat a zero length string the same as a reset
    if(!strLen || (strLen == 1 && str[0] == '0')) {
        this->colorIndices[ColorIndex::Foreground] = 0xF;
        this->colorIndices[ColorIndex::Background] = 0x0;

        return;
    }

    // other comamnd
    auto cmd = StrToInt(str);

    // fg color (first set)
    if(cmd >= 30 && cmd <= 37) {
        this->colorIndices[ColorIndex::Foreground] = cmd - 30;
    }
    // bg color (first set)
    else if(cmd >= 40 && cmd <= 47) {
        this->colorIndices[ColorIndex::Background] = cmd - 40;
    }
    // fg color (second set)
    if(cmd >= 90 && cmd <= 97) {
        this->colorIndices[ColorIndex::Foreground] = cmd - 90;
    }
    // bg color (second set)
    else if(cmd >= 100 && cmd <= 107) {
        this->colorIndices[ColorIndex::Background] = cmd - 100;
    }
}

/**
 * Processes the "set cursor position" command.
 *
 * Its form is CSI n;mH where n is the row, and m is the column. These numbers are 1 based, and
 * default to 1 when omitted, rather than 0.
 */
void FramebufferConsole::processAnsiCup() {
    bool isRow = true;
    size_t numberRead = 0;
    char number[8]{0};
    auto readPtr = this->ansiBuf;

    // read cursor positions
    this->ansiBuf[this->ansiBufUsed - 1] = '\0';

    while(const auto ch = *readPtr++) {
        // semicolon terminates sequence
        if(ch == ';') {
            // convert to integer
            auto val = StrToInt(number);
            this->cursorPos[isRow ? 1 : 0] = val ? (val - 1) : 0;
            isRow = !isRow;

            memset(number, 0, sizeof(number));
            numberRead = 0;
        }
        // copy the string otherwise
        else {
            number[numberRead++] = ch;
        }
    }

    // handle leftover
    auto val = StrToInt(number);
    this->cursorPos[isRow ? 1 : 0] = val ? (val - 1) : 0;

    log("New cursor pos: %lu %lu", this->cursorPos[0], this->cursorPos[1]);
}



/**
 * Processes a printable character. Most are simply displayed on screen (by indexing into the
 * current font's glyph table) but some characters are treated specially.
 */
void FramebufferConsole::print(const char _ch) {
    // handle any non-printables
    if(_ch == '\n') {
        return this->newLine();
    }

    // print character
    const auto ch = (_ch > this->font->maxGlyph) ? '?' : _ch;
    this->drawChar(ch, this->cursorPos[0], this->cursorPos[1]);

    if(++this->cursorPos[0] == this->bufferChars[Size::Width]) {
        this->newLine();
    }
}

/**
 * Draws a character from the current font at the given display position.
 */
void FramebufferConsole::drawChar(const char ch, const size_t x, const size_t y) {
    const uint8_t *fontData = this->font->data + (ch * this->font->stride);
    const auto yOff = this->font->height * y * this->bufferStride;

    auto ptr = this->buffer + (x * this->font->width);
    ptr = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(ptr) + yOff);

    // get colors for fg/bg
    const auto fg = this->colors[ColorIndex::Foreground];
    const auto bg = this->colors[ColorIndex::Background];

    // write character
    for(size_t y = 0; y < this->font->height; y++) {
        auto rowPtr = ptr;

        for(size_t x = 0; x < this->font->width; x++) {
            const uint8_t bit = (1 << (x % 8));
            *rowPtr++ = (*fontData & bit) ? fg : bg;

            // advance to the next cell in font data
            if(x && !(x % 8)) {
                fontData++;
            }
        }

        // advance to next line
        fontData++; // XXX: this will break for fonts not multiples of 8 pixels wide!
        ptr = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(ptr) + this->bufferStride);
    }
}

/**
 * Advance to next line, scrolling the screen if needed.
 */
void FramebufferConsole::newLine() {
    this->cursorPos[0] = 0;
    if(++this->cursorPos[1] == this->bufferChars[Size::Height]) {
        // scroll the entire screen up
        const auto yOff = this->font->height * kScrollAmount * this->bufferStride;
        const auto toMove = this->bufferStride * (this->bufferChars[Size::Height] - kScrollAmount) * this->font->height;

        auto toPtr = this->buffer;
        auto fromPtr = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(toPtr) + yOff);

        memcpy(toPtr, fromPtr, toMove);

        // clear the newly uncovered area
        const auto bg = this->colors[ColorIndex::Background];
        auto clearPtr = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(toPtr) + toMove);

        for(size_t y = 0; y < (kScrollAmount * this->font->height); y++) {
            auto rowPtr = clearPtr;

            for(size_t x = 0; x < this->bufferSize[Size::Width]; x++) {
                *rowPtr++ = bg;
            }

            clearPtr = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(clearPtr) + this->bufferStride);
        }

        // update cursor position
        this->cursorPos[1] -= kScrollAmount;
    }
}



/**
 * Converts the current palette indices into pixel colors.
 */
void FramebufferConsole::updateColors() {
    for(size_t i = 0; i < ColorIndex::Maximum; i++) {
        const auto &color = this->palette[this->colorIndices[i] % kColorPaletteEntries];
        this->colors[i] = color.convert(this->bufferFormat);
    }
}

/**
 * Clears the entire screen.
 */
void FramebufferConsole::clear() {
    auto ptr = this->buffer;
    const auto bg = this->colors[ColorIndex::Background];

    for(size_t y = 0; y < this->bufferSize[Size::Height]; y++) {
        auto rowPtr = ptr;

        for(size_t x = 0; x < this->bufferSize[Size::Width]; x++) {
            *rowPtr++ = bg;
        }

        // go to next row
        ptr = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(ptr) + this->bufferStride);
    }
}
