# Third Party Libraries
This directory contains various third-party libraries that make up the userspace component of the OS. These are automatically built by the CMake file.

- [bzip2](https://sourceware.org/bzip2/): Data compression
- [Cap'n Proto](https://capnproto.org): Serialization and RPC for standard system processes
- [fmtlib](https://fmt.dev/latest/index.html): Nice C++ string formatting
- [ICU](https://unicode-org.github.io/icu): Unicode normalization and other internationalization services
    - This library is _not_ built automatically with everything else. Follow the [cross compilation instructions](https://unicode-org.github.io/icu/userguide/icu4c/build#how-to-cross-compile-icu) to produce the binaries and manually copy them into the sysroot. Failure to follow this step will result in compile or link failures for many applications.
    - You likely will have to specify the absolute path to C/CXX tools to the configure step. Also, be sure that the CFLAGS/CXXFLAGS/LDFLAGS includes the path to the sysroot and the appropriate target. It also is likely necessary to disable building tools, extras, and samples to avoid some linker errors for not yet implemented C library functions.
    - Copy the contents of the `lib` directory in the cross compile build to `$SYSROOT/lib`, and the 
- [libcrc](https://www.libcrc.org): CRC calculation library by Lammert Bies (_Note:_ You need to run `make` inside this directory to generate the CRC lookup tables.)
- [libjpeg-turbo](https://libjpeg-turbo.org): SIMD accelerated JPEG encoder/decoder
- [libpng](http://www.libpng.org/pub/png/libpng.html): Reference implementation of the PNG format as a encoder/decoder. (_Note:_ You will likely have to manually disable building the `pngfix` utility.)
- [lzfse](https://github.com/lzfse/lzfse): C implementation of Apple's LZFSE compression algorithm, optimized for power use.
- [mpack](https://github.com/ludocode/mpack): Fast C MessagePack implementation.
- [OpenLibm](https://github.com/JuliaMath/openlibm): Math library, to serve as an equivalent to libm.
- [zlib](https://www.zlib.net): Compression library, used by libpng.

The `HppOnly` directory contains header-only C++ libraries that are used by various components of the system. Those services that do require them will refer to the header files at this path.
