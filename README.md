# kush os

## Building
We use CMake to build everything. The single `CMakeLists.txt` in this directory can build the kernel, all userspace components, and tools automatically. You will have to select one of the toolchains to use and pass it to CMake in the `CMAKE_TOOLCHAIN_FILE` flag.
