###############################################################################
# Driver manager
#
# Handles discovery of devices on the system, loading drivers for them, and
# maintains an in-memory tree of their relationships as the system runs.
###############################################################################
add_executable(driverman
    src/main.cpp
    src/Log.cpp
    src/experts/Expert.cpp
    src/forest/Forest.cpp
)

if(${KERNEL_ARCH} STREQUAL "x86")
target_sources(driverman PRIVATE
    src/experts/X86PcExpert.cpp
)
endif()

#####
# allow link time optimization
target_compile_options(driverman PRIVATE "-flto")

# search our codebase for includes
target_include_directories(driverman PRIVATE include)
target_include_directories(driverman PRIVATE src)

# required libraries
add_dependencies(driverman c_dynamic system_dynamic)
target_link_libraries(driverman PRIVATE rpc driver)

# we can remove this when it gets implemented :D
target_compile_definitions(driverman PRIVATE -DCLI11_HAS_FILESYSTEM=0)
target_link_libraries(driverman PRIVATE CLI11::CLI11)

# install it to boot directory in sysroot
install(TARGETS driverman RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

