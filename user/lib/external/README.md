# Third Party Libraries
This directory contains various third-party libraries that make up the userspace component of the OS. These are automatically built by the CMake file.

- [bzip2](https://sourceware.org/bzip2/): Data compression
- [cairo](https://www.cairographics.org): General purpose graphics library
    - Manually cross compile the library by invoking `configure` with the correct flags.
    - This directory currently contains the source distributable for version 1.17.4 of the library.
- [fmtlib](https://fmt.dev/latest/index.html): Nice C++ string formatting
- [FreeType](https://www.freetype.org/index.html): TTF (and more!) font rendering
- [ICU](https://unicode-org.github.io/icu): Unicode normalization and other internationalization services
    - This library is _not_ built automatically with everything else. Follow the [cross compilation instructions](https://unicode-org.github.io/icu/userguide/icu4c/build#how-to-cross-compile-icu) to produce the binaries and manually copy them into the sysroot. Failure to follow this step will result in compile or link failures for many applications.
    - You likely will have to specify the absolute path to C/CXX tools to the configure step. Also, be sure that the CFLAGS/CXXFLAGS/LDFLAGS includes the path to the sysroot and the appropriate target. It also is likely necessary to disable building tools, extras, and samples to avoid some linker errors for not yet implemented C library functions.
    - Copy the contents of the `lib` directory in the cross compile build to `$SYSROOT/lib`, and the 
- [libcrc](https://www.libcrc.org): CRC calculation library by Lammert Bies
    - You need to run `make` inside this directory to generate the CRC lookup tables. The library itself is built by the CMake file.
- [libjpeg-turbo](https://libjpeg-turbo.org): SIMD accelerated JPEG encoder/decoder
- [libpng](http://www.libpng.org/pub/png/libpng.html): Reference implementation of the PNG format as a encoder/decoder.
    - You will likely have to manually disable building the `pngfix` utility.
- [lzfse](https://github.com/lzfse/lzfse): C implementation of Apple's LZFSE compression algorithm, optimized for power use.
- [mpack](https://github.com/ludocode/mpack): Fast C MessagePack implementation.
- [OpenLibm](https://github.com/JuliaMath/openlibm): Math library, to serve as an equivalent to libm.
- [pixman](http://pixman.org): Low level image manipulation library; mostly here because cairo requires it
    - You need to manually cross compile it; this involves invoking `configure` with the correct compiler flags. Once complete, copy the static libraries out of `pixman/.libs/` and into the sysroot.
    - This directory currently contains the source distributable for version 0.40 of the library.
- [zlib](https://www.zlib.net): Compression library, used by libpng.

The `HppOnly` directory contains header-only C++ libraries that are used by various components of the system. Those services that do require them will refer to the header files at this path.

## Make-based libraries
For libraries with `configure` files and traditional Make-based build systems (cairo, pixman, etc.) the configure command will likely have a form similar to the following:

```
./configure --host=amd64-pc-kush --target=amd64-pc-kush-elf CC=/toolchain/llvm/bin/clang RANLIB=/toolchain/llvm/bin/llvm-ranlib CXX=/toolchain/llvm/bin/clang++ LD=/toolchain/llvm/bin/ld.lld
```

This goes in addition to any project specific flags; and the host path and target need to be updated to suit.
