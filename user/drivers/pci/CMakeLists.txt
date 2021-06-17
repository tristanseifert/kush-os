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

###############################################################################
# PCI Driver support static library
#
# Any drivers that communicate with devices on a PCI bus should link to this
# static library. It provides the PCI user client, and some abstractions to
# make dealing with devices less of a pain in the ass.
add_library(libpci STATIC
    libpci/src/Helpers.cpp
    # user client
    libpci/src/userclient/PciDriverUser.capnp.c++
    libpci/src/userclient/Client_PciDriverUser.cpp
    libpci/src/userclient/Client.cpp
    # object wrappers
    libpci/src/Device.cpp
)
set_target_properties(libpci PROPERTIES OUTPUT_NAME "pci")

target_include_directories(libpci PUBLIC libpci/include)
target_include_directories(libpci PRIVATE libpci/src)

target_compile_options(libpci PRIVATE -flto)
target_link_libraries(libpci PUBLIC capnp driver rpc mpack_dynamic)

# the PCI server needs to link against LibPci so it can get the RPC serialization functions
target_link_libraries(pcisrv PRIVATE libpci)
