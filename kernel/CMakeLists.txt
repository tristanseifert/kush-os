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

# kernel version
configure_file(${CMAKE_CURRENT_LIST_DIR}/src/version.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
set(version_file "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

# build the rest of the kernel
add_executable(kernel
    src/init.cpp
    src/c/stack_guard.c
    src/c/printf.c
    src/c/string.c
    src/mem/AnonPool.cpp
    src/mem/PhysicalAllocator.cpp
    src/mem/StackPool.cpp
    src/mem/Heap.cpp
    src/mem/liballoc/liballoc.cpp
    src/runtime/CppRuntime.cpp
    src/runtime/log.cpp
    src/vm/Mapper.cpp
    src/vm/Map.cpp
    src/sched/Scheduler.cpp
    src/sched/Task.cpp
    src/sched/Thread.cpp
    src/sched/IdleWorker.cpp
    ${version_file}
)

target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(kernel PRIVATE ${ARCH_INCLUDE_DIR})
target_include_directories(kernel PRIVATE ${PLATFORM_INCLUDE_DIR})
target_include_directories(kernel PRIVATE src)

target_link_libraries(kernel PRIVATE ${ARCH_TARGET_NAME})
target_link_libraries(kernel PRIVATE ${PLATFORM_TARGET_NAME})

# link the compiler-specific libraries
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_link_libraries(kernel PRIVATE gcc)
endif()

# set filename for the resulting kernel binary
set_target_properties(kernel PROPERTIES OUTPUT_NAME "kernel-${KERNEL_ARCH}-${KERNEL_PLATFORM}")
set_target_properties(kernel PROPERTIES SUFFIX "${PLATFORM_KERNEL_EXTENSION}")

# configure the linker script
message("Platform linker script: ${PLATFORM_LINKER_SCRIPT}")
set_target_properties(kernel PROPERTIES LINK_DEPENDS ${PLATFORM_LINKER_SCRIPT})
string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,-T,${PLATFORM_LINKER_SCRIPT}")

