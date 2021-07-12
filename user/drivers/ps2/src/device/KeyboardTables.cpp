#include "Keyboard.h"

#include "rpc/Scancodes.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

/**
 * Conversion table for the primary PS/2 scan code set
 */
const std::unordered_map<std::byte, Scancode> Keyboard::kScancodePrimary{
    {std::byte{0x01}, Scancode::F9},
    {std::byte{0x03}, Scancode::F5},
    {std::byte{0x04}, Scancode::F3},
    {std::byte{0x05}, Scancode::F1},
    {std::byte{0x06}, Scancode::F2},
    {std::byte{0x07}, Scancode::F12},
    {std::byte{0x09}, Scancode::F10},
    {std::byte{0x0A}, Scancode::F8},
    {std::byte{0x0B}, Scancode::F6},
    {std::byte{0x0C}, Scancode::F4},
    {std::byte{0x0D}, Scancode::Tab},
    {std::byte{0x0E}, Scancode::Backtick},

    {std::byte{0x11}, Scancode::LeftAlt},
    {std::byte{0x12}, Scancode::LeftShift},
    {std::byte{0x14}, Scancode::LeftCtrl},
    {std::byte{0x15}, Scancode::Q},
    {std::byte{0x16}, Scancode::Num1},
    {std::byte{0x1A}, Scancode::Z},
    {std::byte{0x1B}, Scancode::S},
    {std::byte{0x1C}, Scancode::A},
    {std::byte{0x1D}, Scancode::W},
    {std::byte{0x1E}, Scancode::Num2},

    {std::byte{0x21}, Scancode::C},
    {std::byte{0x22}, Scancode::X},
    {std::byte{0x23}, Scancode::D},
    {std::byte{0x24}, Scancode::E},
    {std::byte{0x25}, Scancode::Num4},
    {std::byte{0x26}, Scancode::Num3},
    {std::byte{0x29}, Scancode::Space},
    {std::byte{0x2A}, Scancode::V},
    {std::byte{0x2B}, Scancode::F},
    {std::byte{0x2C}, Scancode::T},
    {std::byte{0x2D}, Scancode::R},
    {std::byte{0x2E}, Scancode::Num5},

    {std::byte{0x31}, Scancode::N},
    {std::byte{0x32}, Scancode::B},
    {std::byte{0x33}, Scancode::H},
    {std::byte{0x34}, Scancode::G},
    {std::byte{0x35}, Scancode::Y},
    {std::byte{0x36}, Scancode::Num6},
    {std::byte{0x3A}, Scancode::M},
    {std::byte{0x3B}, Scancode::J},
    {std::byte{0x3C}, Scancode::U},
    {std::byte{0x3D}, Scancode::Num7},
    {std::byte{0x3E}, Scancode::Num8},

    {std::byte{0x41}, Scancode::Comma},
    {std::byte{0x42}, Scancode::K},
    {std::byte{0x43}, Scancode::I},
    {std::byte{0x44}, Scancode::O},
    {std::byte{0x45}, Scancode::Num0},
    {std::byte{0x46}, Scancode::Num9},
    {std::byte{0x49}, Scancode::Period},
    {std::byte{0x4A}, Scancode::ForwardSlash},
    {std::byte{0x4B}, Scancode::L},
    {std::byte{0x4C}, Scancode::Semicolon},
    {std::byte{0x4D}, Scancode::P},
    {std::byte{0x4E}, Scancode::Hyphen},

    {std::byte{0x52}, Scancode::Apostrophe},
    {std::byte{0x54}, Scancode::OpenBracket},
    {std::byte{0x55}, Scancode::Equals},
    {std::byte{0x58}, Scancode::CapsLock},
    {std::byte{0x59}, Scancode::RightShift},
    {std::byte{0x5A}, Scancode::Enter},
    {std::byte{0x5B}, Scancode::CloseBracket},
    {std::byte{0x5D}, Scancode::Backslash},

    {std::byte{0x66}, Scancode::Backspace},
    {std::byte{0x69}, Scancode::Keypad1},
    {std::byte{0x6B}, Scancode::Keypad4},
    {std::byte{0x6C}, Scancode::Keypad7},

    {std::byte{0x70}, Scancode::Keypad0},
    {std::byte{0x71}, Scancode::KeypadPeriod},
    {std::byte{0x72}, Scancode::Keypad2},
    {std::byte{0x73}, Scancode::Keypad5},
    {std::byte{0x74}, Scancode::Keypad6},
    {std::byte{0x75}, Scancode::Keypad8},
    {std::byte{0x76}, Scancode::Escape},
    {std::byte{0x77}, Scancode::NumLock},
    {std::byte{0x78}, Scancode::F11},
    {std::byte{0x79}, Scancode::KeypadPlus},
    {std::byte{0x7A}, Scancode::Keypad3},
    {std::byte{0x7B}, Scancode::KeypadHyphen},
    {std::byte{0x7C}, Scancode::KeypadAsterisk},
    {std::byte{0x7D}, Scancode::Keypad9},
    {std::byte{0x7E}, Scancode::ScrollLock},

    {std::byte{0x83}, Scancode::F7},
};


/**
 * Conversion table for the alternate PS/2 scan code set; this is scan codes that are prefixed with
 * an 0xE0 byte.
 */
const std::unordered_map<std::byte, Scancode> Keyboard::kScancodeAlternate{
    {std::byte{0x11}, Scancode::RightAlt},
    {std::byte{0x14}, Scancode::RightCtrl},
    {std::byte{0x1F}, Scancode::LeftMeta},

    {std::byte{0x27}, Scancode::RightMeta},

    {std::byte{0x37}, Scancode::Power},

    {std::byte{0x4A}, Scancode::KeypadSlash},

    {std::byte{0x5A}, Scancode::KeypadEnter},

    {std::byte{0x69}, Scancode::End},
    {std::byte{0x6B}, Scancode::ArrowLeft},
    {std::byte{0x6C}, Scancode::Home},

    {std::byte{0x70}, Scancode::Insert},
    {std::byte{0x71}, Scancode::ForwardDelete},
    {std::byte{0x72}, Scancode::ArrowDown},
    {std::byte{0x74}, Scancode::ArrowRight},
    {std::byte{0x75}, Scancode::ArrowUp},
    {std::byte{0x7A}, Scancode::PageDown},
    {std::byte{0x7D}, Scancode::PageUp},
};

