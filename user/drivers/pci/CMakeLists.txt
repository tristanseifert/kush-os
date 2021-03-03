###############################################################################
# PCI Bus server
#
# Handles discovery of devices on the PCI bus, resource allocation, device
# interrupts, and other such fun stuff. Each root bridge on the system creates
# an instance of this driver.
###############################################################################
add_executable(pcisrv
    src/main.cpp
    src/Log.cpp
    src/bus/PciConfig.cpp
    src/bus/RootBridge.cpp
)

target_include_directories(pcisrv PRIVATE src/acpica/include)

# allow link time optimization
target_compile_options(pcisrv PRIVATE "-flto")

# search our codebase for includes
target_include_directories(pcisrv PRIVATE include)
target_include_directories(pcisrv PRIVATE src)

# required libraries
add_dependencies(pcisrv c_dynamic system_dynamic)
target_link_libraries(pcisrv PRIVATE rpc driver mpack_dynamic)

# install it to boot directory in sysroot
install(TARGETS pcisrv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

