###############################################################################
# PCI Bus server
#
# Handles discovery of devices on the PCI bus, resource allocation, device
# interrupts, and other such fun stuff. Each root bridge on the system creates
# an instance of this driver.
###############################################################################
add_executable(pcisrv
    src/main.cpp
    src/BusRegistry.cpp
    src/Log.cpp
    src/bus/pcie/PciExpressBus.cpp
    src/bus/pcie/Device.cpp
    src/bus/pcie/ConfigSpaceReader.cpp
    src/rpc/Server.cpp
    src/rpc/PciDriverUser.capnp.c++
    src/rpc/Server_PciDriverUser.cpp
)

target_include_directories(pcisrv PRIVATE src/acpica/include)

# allow link time optimization
target_compile_options(pcisrv PRIVATE -flto)

# search our codebase for includes
target_include_directories(pcisrv PRIVATE include)
target_include_directories(pcisrv PRIVATE src)

# required libraries
add_dependencies(pcisrv c_dynamic)
target_link_libraries(pcisrv PRIVATE capnp driver rpc mpack_dynamic fmt::fmt)

# install it to boot directory in sysroot
install(TARGETS pcisrv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)
install(TARGETS pcisrv RUNTIME DESTINATION ${SYSROOT_DIR}/boot/sbin)

