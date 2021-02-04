###############################################################################
# x86 (32-bit Intel) architecture support
###############################################################################

add_library(kernel_arch_x86 OBJECT
    src/init.cpp
    src/exceptions.cpp
    src/exceptions.S
    src/gdt.cpp
    src/gdt.S
    src/idt.cpp
    src/vm/PTEHandler.cpp
    src/vm/PDPTPool.cpp
)

target_include_directories(kernel_arch_x86 PUBLIC include)
target_include_directories(kernel_arch_x86 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_arch_x86 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)

set(ARCH_TARGET_NAME "kernel_arch_x86" CACHE STRING "Architecture target name" FORCE)
set(ARCH_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Architecture include directory" FORCE)
