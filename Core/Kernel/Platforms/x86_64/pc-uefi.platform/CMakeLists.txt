###################################################################################################
# 64-bit x86 UEFI platform
###################################################################################################

##### Build the platform code
add_library(platform-amd64-uefi OBJECT
    Sources/Boot/Entry.cpp
    Sources/Boot/Header.cpp
    Sources/Arch/Processor.cpp
    Sources/Io/Console.cpp
    Sources/Util/Backtrace.cpp
)

target_include_directories(platform-amd64-uefi PUBLIC ${CMAKE_CURRENT_LIST_DIR}/Includes)
target_include_directories(platform-amd64-uefi PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Sources)
target_include_directories(platform-amd64-uefi PRIVATE ${CMAKE_CURRENT_LIST_DIR}/External/stivale)
target_include_directories(platform-amd64-uefi PRIVATE ${KERNEL_LIST_DIR}/Includes)

##### Define the expected platform defines
set(KERNEL_PLATFORM_FILE_EXTENSION ".elf" PARENT_SCOPE)
set(KERNEL_PLATFORM_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/kernel.ld" PARENT_SCOPE)
set(KERNEL_PLATFORM_LINKER_OPTS "" PARENT_SCOPE)

set(KERNEL_PLATFORM_LIB platform-amd64-uefi PARENT_SCOPE)

# disable SIMD in the kernel (we can still use intrinsics as needed)
set(COMPILE_OPTS -mno-red-zone -fPIC -fPIE -fno-omit-frame-pointer
    -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx)
set(KERNEL_PLATFORM_COMPILE_OPTS -mcmodel=kernel ${COMPILE_OPTS} PARENT_SCOPE)

# Ensure we're built with the appropriate options also
# XXX: why do we need to use mcmodel=large here? Otherwise we get link errors…
target_compile_options(platform-amd64-uefi PRIVATE -mcmodel=large ${COMPILE_OPTS})
