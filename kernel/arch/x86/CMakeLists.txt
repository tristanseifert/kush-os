###############################################################################
# x86 (32-bit Intel) architecture support
###############################################################################

add_library(kernel_arch_x86 OBJECT
    src/init.c
    src/exceptions.c
    src/exceptions.S
    src/gdt.c
    src/gdt.S
    src/idt.c
    src/vm/PTEHandler.cpp
)

target_include_directories(kernel_arch_x86 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_arch_x86 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)

set(ARCH_TARGET_NAME "kernel_arch_x86" CACHE STRING "Architecture target name" FORCE)
set(ARCH_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Architecture include directory" FORCE)
