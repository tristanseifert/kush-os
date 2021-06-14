# Third Party Libraries
This directory contains various third-party libraries that make up the userspace component of the OS. These are automatically built by the CMake file.

- [OpenLibm](https://github.com/JuliaMath/openlibm): Math library, to serve as an equivalent to libm.
- [mpack](https://github.com/ludocode/mpack): Fast C MessagePack implementation.
- [Cap'n Proto](https://capnproto.org): Serialization and RPC for standard system processes

The `HppOnly` directory contains header-only C++ libraries that are used by various components of the system. Those services that do require them will refer to the header files at this path.
