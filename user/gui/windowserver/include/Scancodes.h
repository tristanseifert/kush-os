#pragma once

#include <cstdint>

/**
 * Defines the window server's scan code set; this is essentially the same as the codes specified
 * in the USB HID Usage Tables specification for the Keyboard/Keyboard Page.
 */
enum class Scancode: uint32_t {
    A                                   = 0x04,
    B                                   = 0x05,
    C                                   = 0x06,
    D                                   = 0x07,
    E                                   = 0x08,
    F                                   = 0x09,
    G                                   = 0x0A,
    H                                   = 0x0B,
    I                                   = 0x0C,
    J                                   = 0x0D,
    K                                   = 0x0E,
    L                                   = 0x0F,
    M                                   = 0x10,
    N                                   = 0x11,
    O                                   = 0x12,
    P                                   = 0x13,
    Q                                   = 0x14,
    R                                   = 0x15,
    S                                   = 0x16,
    T                                   = 0x17,
    U                                   = 0x18,
    V                                   = 0x19,
    W                                   = 0x1A,
    X                                   = 0x1B,
    Y                                   = 0x1C,
    Z                                   = 0x1D,

    Num1                                = 0x1E,
    Num2                                = 0x1F,
    Num3                                = 0x20,
    Num4                                = 0x21,
    Num5                                = 0x22,
    Num6                                = 0x23,
    Num7                                = 0x24,
    Num8                                = 0x25,
    Num9                                = 0x26,
    Num0                                = 0x27,

    Backspace                           = 0x2A,
    ForwardDelete                       = 0x4C,

    Enter                               = 0x28,

    Escape                              = 0x29,
    Power                               = 0x66,

    CapsLock                            = 0x39,
    NumLock                             = 0x53,

    Tab                                 = 0x2B,
    Space                               = 0x2C,

    PrintScreen                         = 0x46,
    ScrollLock                          = 0x47,
    Pause                               = 0x48,
    Insert                              = 0x49,

    Home                                = 0x4A,
    End                                 = 0x4D,
    PageUp                              = 0x4B,
    PageDown                            = 0x4E,

    ArrowRight                          = 0x4F,
    ArrowLeft                           = 0x50,
    ArrowDown                           = 0x51,
    ArrowUp                             = 0x52,

    /*
     * The next block of scancodes defines various modifier keys on the keyboard. There may be
     * multiple versions of a modifier key but they usually are treated the same.
     *
     * The "Meta" keys refer to the "Windows logo" key on the keyboard.
     */
    LeftCtrl                            = 0xE0,
    LeftShift                           = 0xE1,
    LeftAlt                             = 0xE2,
    LeftMeta                            = 0xE3,
    RightCtrl                           = 0xE4,
    RightShift                          = 0xE5,
    RightAlt                            = 0xE6,
    RightMeta                           = 0xE7,

    // single dash (-) and underscore
    Hyphen                              = 0x2D,
    // equal sign and plus sign
    Equals                              = 0x2E,
    // opening square and curly brackets
    OpenBracket                         = 0x2F,
    // closing square and curly brackets
    CloseBracket                        = 0x30,
    // backslash and pipe
    Backslash                           = 0x31,
    // semicolon and colon
    Semicolon                           = 0x33,
    // single and double quotes
    Apostrophe                          = 0x34,
    // grave accent (backtick) and tilde
    Backtick                            = 0x35,
    // comma and less than (<) symbol
    Comma                               = 0x36,
    // period and greater than (>) symbol
    Period                              = 0x37,
    // Slash and question mark
    ForwardSlash                        = 0x38,

    F1                                  = 0x3A,
    F2                                  = 0x3B,
    F3                                  = 0x3C,
    F4                                  = 0x3D,
    F5                                  = 0x3E,
    F6                                  = 0x3F,
    F7                                  = 0x40,
    F8                                  = 0x41,
    F9                                  = 0x42,
    F10                                 = 0x43,
    F11                                 = 0x44,
    F12                                 = 0x45,
    F13                                 = 0x68,
    F14                                 = 0x69,
    F15                                 = 0x6A,
    F16                                 = 0x6B,
    F17                                 = 0x6C,
    F18                                 = 0x6D,
    F19                                 = 0x6E,
    F20                                 = 0x6F,
    F21                                 = 0x70,
    F22                                 = 0x71,
    F23                                 = 0x72,
    F24                                 = 0x73,

    /*
     * Below are keys that occur on the key pad, or number pad, of the keyboard. They have separate
     * scan codes so we can distinguish them, but for the most part, they do the same thing as
     * their normal counterparts.
     */
    KeypadNumLock                       = 0x53,
    KeypadEnter                         = 0x58,

    KeypadSlash                         = 0x54,
    KeypadAsterisk                      = 0x55,
    KeypadHyphen                        = 0x56,
    KeypadPlus                          = 0x57,
    // also doubles as a delete key
    KeypadPeriod                        = 0x63,
    KeypadEquals                        = 0x67,

    // 1/End
    Keypad1                             = 0x59,
    // 2/Down arrow
    Keypad2                             = 0x5A,
    // 3/Page down
    Keypad3                             = 0x5B,
    // 4/Left arrow
    Keypad4                             = 0x5C,
    Keypad5                             = 0x5D,
    // 6/Right arrow
    Keypad6                             = 0x5E,
    // 7/Home
    Keypad7                             = 0x5F,
    // 8/Up arrow
    Keypad8                             = 0x60,
    // 9/Page up
    Keypad9                             = 0x61,
    // 0/insert
    Keypad0                             = 0x62,
};
