#ifndef DEBUG_FRAMEBUFFERCONSOLE_H
#define DEBUG_FRAMEBUFFERCONSOLE_H

#include <cstddef>
#include <cstdint>

namespace debug {
struct BitmapFont;

/**
 * A basic bitmap framebuffer text console, with some support for ANSI escape sequences.
 *
 * This is primarily intended as a debugging aid, and its setup is platform specific. It is not
 * reentrant or thread safe, so you will have to add locking around calls into it if it's to be
 * used from multiple cores.
 *
 * @note This currently only works for 32-bit framebuffers, though they may be any arrangement of
 *       colors, e.g. ARGB or RGBA.
 */
class FramebufferConsole {
    public:
        /**
         * Defines the ordering of a framebuffer's components, and how they are packed into a
         * single pixel word.
         */
        enum class ColorOrder {
            RGBA,
            ARGB,
        };

        /**
         * Small encapsulation for a color, which can be converted to a 32-bit value to write into
         * the framebuffer.
         */
        struct Color {
            Color() : r(0), g(0), b(0), a(0xFF) {}
            Color(const uint8_t _r, const uint8_t _g, const uint8_t _b, const uint8_t _a = 0xFF) :
                r(_r), g(_g), b(_b), a(_a) {}

            /// Converts a color object to a framebuffer pixel value
            uint32_t convert(const ColorOrder o) const {
                switch(o) {
                    case ColorOrder::RGBA:
                        return (this->r << 24) | (this->g << 16) | (this->b << 8) | this->a;
                    case ColorOrder::ARGB:
                        return (this->a << 24) | (this->r << 16) | (this->g << 8) | this->b;
                }
            }

            // components of the color
            uint8_t r, g, b, a;
        };

    public:
        /// Initialize console with the given framebuffer
        FramebufferConsole(uint32_t *fb, const ColorOrder format, const size_t w, const size_t h,
                const size_t stride = 0);

        /// Writes a single character to the console
        void write(const char ch);
        /// Writes the given string to the console
        void write(const char *str, const size_t length = 0);

    private:
        /// Indices into buffer size array
        enum Size {
            Width       = 0,
            Height      = 1,
        };
        /// Indices into the color arrays
        enum ColorIndex {
            Foreground  = 0,
            Background  = 1,

            Maximum
        };

        /// Print state machine state
        enum class WriteState {
            /**
             * Idle state: receive a character, and print it if it's a printable character; if it
             * is the start of an escape sequence, switch into that state.
             */
            Idle,
            /**
             * Received the first byte ('\e') of an ANSI escape sequence. The received character
             * should determine further what type of escape we received.
             */
            AnsiEscapeStart,
            /**
             * Currently reading an ANSI CSI sequence
             */
            AnsiReadingCSI,
        };

    private:
        /// Converts palette indices into pixel values
        void updateColors();

        /// Processes the currently buffered ANSI escape sequence
        void processAnsi();
        /// Processes a graphic mode escape
        void processAnsiSgr();
        /// Process a single SGR attribute
        void processAnsiSgr(const char *str, const size_t len);
        /// Set the cursor position
        void processAnsiCup();

        /// Prints a character on screen
        void print(const char ch);
        /// Draws a printable character to the given character cell on screen.
        void drawChar(const char ch, const size_t x, const size_t y);
        /// Advances to the next line, optionally scrolling the screen if required
        void newLine();
        /// Clears the entire screen to the current background color
        void clear();

    private:
        /// number of entries in color palette
        constexpr static const size_t kColorPaletteEntries = 16;
        /// size of the ANSI escape sequence buffer
        constexpr static const size_t kAnsiBufSize = 32;

        /// how many lines of text are discarded when scrolling up (because we've hit the bottom)
        constexpr static const size_t kScrollAmount = 5;

    private:
        /// framebuffer head pointer
        uint32_t *buffer;
        /// dimensions of framebuffer (in pixels)
        size_t bufferSize[2];
        /// number of bytes between rows
        size_t bufferStride;
        /// component order of the framebuffer
        ColorOrder bufferFormat;

        /// dimensions of framebuffer (in characters)
        size_t bufferChars[2];

        /// current fore and background palette indices
        uint8_t colorIndices[ColorIndex::Maximum] = {0xF, 0x0};
        /// current fore and background colors (pixel values)
        uint32_t colors[ColorIndex::Maximum];

        /// default 16 entry color palette
        Color palette[kColorPaletteEntries] = {
            // black, red, green, yellow
            Color(0, 0, 0),       Color(205, 49, 49),   Color(13, 188, 121), Color(229, 229, 16),
            // blue, magenta, cyan, white
            Color(36, 114, 200),  Color(188, 63, 188),  Color(17, 168, 205), Color(229, 229, 229),
            // gray, bright red, bright green, bright yellow
            Color(102, 102, 102), Color(241, 76, 76),   Color(35, 209, 139), Color(245, 245, 67),
            // bright blue, bright magenta, bright cyan, bright white
            Color(59, 142, 234),  Color(214, 112, 214), Color(41, 184, 219), Color(229, 229, 229),
        };

        // current cursor position
        size_t cursorPos[2] = {0, 0};

        /// current bitmap font for drawing
        const BitmapFont *font;

        /// current state for the write machine
        WriteState writeState = WriteState::Idle;
        /// number of bytes of the escape code buffer used
        size_t ansiBufUsed = 0;
        /// buffer for temporarily holding escape sequences
        char ansiBuf[kAnsiBufSize];
};
}

#endif
