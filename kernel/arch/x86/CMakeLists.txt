###############################################################################
# x86 (32-bit Intel) architecture support
###############################################################################

add_library(kernel_arch_x86 OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/src/init.c
    ${CMAKE_CURRENT_LIST_DIR}/src/exceptions.c
    ${CMAKE_CURRENT_LIST_DIR}/src/exceptions.S
    ${CMAKE_CURRENT_LIST_DIR}/src/gdt.c
    ${CMAKE_CURRENT_LIST_DIR}/src/gdt.S
    ${CMAKE_CURRENT_LIST_DIR}/src/idt.c
)

target_include_directories(kernel_arch_x86 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)

set(ARCH_TARGET_NAME "kernel_arch_x86" CACHE STRING "Architecture target name" FORCE)
set(ARCH_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Architecture include directory" FORCE)
