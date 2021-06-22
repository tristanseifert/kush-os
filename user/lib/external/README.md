# Third Party Libraries
This directory contains various third-party libraries that make up the userspace component of the OS. These are automatically built by the CMake file.

- [bzip2](https://sourceware.org/bzip2/): Data compression
- [Cap'n Proto](https://capnproto.org): Serialization and RPC for standard system processes
- [fmtlib](https://fmt.dev/latest/index.html): Nice C++ string formatting
- [libcrc](https://www.libcrc.org): CRC calculation library by Lammert Bies (_Note:_ You need to run `make` inside this directory to generate the CRC lookup tables.)
- [lzfse](https://github.com/lzfse/lzfse): C implementation of Apple's LZFSE compression algorithm, optimized for power use.
- [mpack](https://github.com/ludocode/mpack): Fast C MessagePack implementation.
- [OpenLibm](https://github.com/JuliaMath/openlibm): Math library, to serve as an equivalent to libm.

The `HppOnly` directory contains header-only C++ libraries that are used by various components of the system. Those services that do require them will refer to the header files at this path.
