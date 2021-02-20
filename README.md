# kush os

## Building
We use CMake to build everything. The single `CMakeLists.txt` in this directory can build the kernel, all userspace components, and tools automatically. You will have to select one of the toolchains to use and pass it to CMake in the `CMAKE_TOOLCHAIN_FILE` flag.

### Directory structure
- kernel: Kernel source files, including architecture/platform specific stuff
    - arch: Architecture initialization, exceptions, interrupts, syscalls
    - platform: Booting and entry point, timers
- user: All userspace code
    - libs: Libraries that get put in the userspace image
        - external: Third party libraries (most go in /usr/lib)
        - libsystem: System call wrapper
        - libc: C library
- tools: Host side utilities
- sysroot: Base directory of the OS root directory. Automatically built up as kernel/userspace is built.
- toolchain: Created by the `build_toolchain.sh` script.

## LLVM Toolchain
You can use the `build_toolchain.sh` script to build a toolchain to use for compiling the system. This sets up most of the libraries required: you'll also have to build a few supporting libraries.

Before compiling the libraries below, you will likely need to compile and install the C library into the sysroot; these libraries expect to find certain headers there.

### compiler-rt
This contains helper functions and other stuff the compiler will rely on being present in all non-freestanding libraries. By default, the script doesn't seem to compile the compiler-rt for our platform. 

To build it, execute the following from inside the toolchain's sources folder:

```
cd llvm-project
mkdir build-compiler-rt
cd build-compiler-rt
cmake ../compiler-rt -DLLVM_CONFIG_PATH=../../../llvm/bin/llvm-config -DCMAKE_C_COMPILER_TARGET="i386-pc-kush-elf" -DCMAKE_ASM_COMPILER_TARGET="i386-pc-kush-elf" -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DCMAKE_TOOLCHAIN_FILE=~/kush/cmake/toolchain-i386-clang.cmake -Wno-dev -DCMAKE_SIZEOF_VOID_P=4 -DCOMPILER_RT_BAREMETAL_BUILD=ON
make -j40
mkdir -p ../../../llvm/lib/clang/12.0.0/lib/kush 
cp lib/generic/libclang_rt.builtins-i386.a ../../../llvm/lib/clang/12.0.0/lib/kush
```

### libcxxabi and libcxx
libcxxabi provides the lower level portable layer for the C++ runtime library. Build it like so:

```
cd llvm-project
mkdir build-libcxxabi
cd build-libcxxabi
cmake ../libcxxabi -DCMAKE_TOOLCHAIN_FILE=~/kush/cmake/toolchain-i386-clang.cmake -DLLVM_CONFIG_PATH=../../../llvm/bin/llvm-config -DLIBCXX_TARGET_TRIPLE="i386-pc-kush-elf"  -DLIBCXXABI_INSTALL_PREFIX=/Users/tristan/kush/sysroot/ -Wno-dev -DCMAKE_SIZEOF_VOID_P=4 -DLIBCXXABI_ENABLE_THREADS=OFF
make install -j40
```

Once built, you have to build the libcxx, the actual C++ runtime:

```
cd llvm-project
mkdir build-libcxx
cd build-libcxx
cmake ../libcxx -DCMAKE_TOOLCHAIN_FILE=~/kush/cmake/toolchain-i386-clang.cmake -DLLVM_CONFIG_PATH=../../../llvm/bin/llvm-config -DLIBCXX_TARGET_TRIPLE="i386-pc-kush-elf" -DLIBCXX_INSTALL_PREFIX=/Users/tristan/kush/sysroot/ -DLIBCXX_INSTALL_HEADER_PREFIX=/Users/tristan/kush/sysroot/usr/ -DLIBCXX_CXX_ABI=libcxxabi -DLIBCXX_ENABLE_THREADS=OFF -DLIBCXX_ENABLE_STDOUT=OFF -DLIBCXX_ENABLE_STDIN=OFF -DLIBCXX_ENABLE_MONOTONIC_CLOCK=OFF -DLIBCXX_ENABLE_RANDOM_DEVICE=OFF -DLIBCXX_ENABLE_FILESYSTEM=OFF  -DLIBCXX_ENABLE_ABI_LINKER_SCRIPT=OFF
make install -j40
```

Most of the flags set to OFF will eventually be removed, when the C libraries and the system itself implements the requisite features.

### libunwind
Required for stack backtraces and C++ exception support. 


```
cd llvm-project
mkdir build-libunwind
cd build-libunwind
cmake ../libunwind -DCMAKE_TOOLCHAIN_FILE=~/kush/cmake/toolchain-i386-clang.cmake -DLLVM_CONFIG_PATH=../../../llvm/bin/llvm-config -DLIBUNWIND_TARGET_TRIPLE="i386-pc-kush-elf" -DLIBUNWIND_INSTALL_PREFIX=/Users/tristan/kush/sysroot/ -DLIBUNWIND_USE_COMPILER_RT=ON -DLIBUNWIND_ENABLE_SHARED=OFF -DLIBUNWIND_ENABLE_THREADS=OFF
make install -j40
```

We can enable `LIBUNWIND_ENABLE_THREADS` when we've got a pthreads compatibility layer.
