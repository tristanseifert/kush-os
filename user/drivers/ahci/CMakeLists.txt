###############################################################################
# AHCI Driver
#
# Provides access to SATA devices via the standard AHCI controller interface
# exposed via PCI.
###############################################################################
add_executable(ahcidrv
    src/main.cpp
    src/Log.cpp
)

# allow link time optimization
target_compile_options(ahcidrv PRIVATE -flto -fno-rtti)

# search our codebase for includes
target_include_directories(ahcidrv PRIVATE include)
target_include_directories(ahcidrv PRIVATE src)

# required libraries
add_dependencies(ahcidrv c_dynamic system_dynamic)
target_link_libraries(ahcidrv PRIVATE capnp driver rpc mpack_dynamic)

# install it to boot directory in sysroot
install(TARGETS ahcidrv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)
install(TARGETS ahcidrv RUNTIME DESTINATION ${SYSROOT_DIR}/boot/sbin)

