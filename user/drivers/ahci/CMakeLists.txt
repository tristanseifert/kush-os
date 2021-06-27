###############################################################################
# AHCI Driver
#
# Provides access to SATA devices via the standard AHCI controller interface
# exposed via PCI.
###############################################################################
add_executable(ahcidrv
    src/main.cpp
    src/Log.cpp
    # AHCI controller implementation
    src/ControllerRegistry.cpp
    src/Controller.cpp
    src/Port.cpp
    # device implementations
    src/device/AtaDisk.cpp
    src/device/AtaDiskRpcServer.cpp
    # helper functions
    src/util/String.cpp
)

# allow link time optimization
target_compile_options(ahcidrv PRIVATE -flto -fno-rtti -fno-exceptions)
target_link_options(ahcidrv PRIVATE -s)

# search our codebase for includes
target_include_directories(ahcidrv PRIVATE include)
target_include_directories(ahcidrv PRIVATE src)

# required libraries
add_dependencies(ahcidrv c_dynamic system_dynamic)
target_link_libraries(ahcidrv PRIVATE driver libpci DriverSupport::Disk rpc mpack concurrentqueue)

# install it to boot directory in sysroot
install(TARGETS ahcidrv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)
install(TARGETS ahcidrv RUNTIME DESTINATION ${SYSROOT_DIR}/boot/sbin)

