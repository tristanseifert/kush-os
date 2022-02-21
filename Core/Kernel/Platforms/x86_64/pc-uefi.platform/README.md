# amd64 UEFI Platform
This platform provides support for booting on any UEFI-compatible 64-bit PC. It expects to be booted with the [Limine bootloader](https://limine-bootloader.org) or any other bootloader that implements the stivale2 protocol.

## Configuration
You can tweak the following settings for this platform:

- 

## Boot Arguments
The kernel's command line can be augmented with the following keys to control the platform at runtime:

- `console=`: Specify additional console devices, such as an IO port or 16650-compatible UART.
