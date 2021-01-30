###############################################################################
# Handles compilation of the kernel.
###############################################################################

# select the correct architecture code and build it
set(KERNEL_ARCH "x86" CACHE STRING "Kernel CPU architecture")
set_property(CACHE KERNEL_ARCH PROPERTY STRINGS x86)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/arch/${KERNEL_ARCH})

# select the correct platform code and build it
set(KERNEL_PLATFORM "pc" CACHE STRING "Kernel system platform")
set_property(CACHE KERNEL_PLATFORM PROPERTY STRINGS pc)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/platform/${KERNEL_PLATFORM})

# build the rest of the kernel
add_executable(kernel
    ${CMAKE_CURRENT_LIST_DIR}/src/boot.S
    ${CMAKE_CURRENT_LIST_DIR}/src/init.c
)

message(${PLATFORM_LINKER_SCRIPT})
set_target_properties(kernel PROPERTIES LINK_DEPENDS ${PLATFORM_LINKER_SCRIPT})

