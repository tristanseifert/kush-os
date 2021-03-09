###############################################################################
# amd64 (64-bit Intel) architecture support
###############################################################################
###############################################################################
# build the actual arch code
add_library(kernel_arch_amd64 OBJECT
    src/init.cpp
    src/exceptions.cpp
    src/exceptions.S
    src/gdt.cpp
    src/gdt.S
    src/idt.cpp
    src/PerCpuInfo.cpp
    src/vm/PTEHandler.cpp
    src/sched/Thread.cpp
    src/sched/switchto.S
    src/syscall/entry.S
    src/syscall/Handler.cpp
    src/syscall/ThreadLocal.cpp
)

set(ARCH_COMPILE_OPTS -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 CACHE STRING "Architecture specific compile flags")

target_compile_options(kernel_arch_amd64 PRIVATE ${KERNEL_COMPILE_OPTS} ${ARCH_COMPILE_OPTS})

target_include_directories(kernel_arch_amd64 PUBLIC include)
target_include_directories(kernel_arch_amd64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_arch_amd64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_include_directories(kernel_arch_amd64 PRIVATE src)

set(ARCH_TARGET_NAME "kernel_arch_amd64" CACHE STRING "Architecture target name" FORCE)
set(ARCH_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Architecture include directory" FORCE)

