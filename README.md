# kush os

## Building
We use CMake to build everything. The single `CMakeLists.txt` in this directory can build the kernel, all userspace components, and tools automatically. You will have to select one of the toolchains to use and pass it to CMake in the `CMAKE_TOOLCHAIN_FILE` flag.

## LLVM Toolchain
You can use the `build_toolchain.sh` script to build a toolchain to use for compiling the system. This sets up most of the libraries required: you'll also have to build a few supporting libraries.

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
