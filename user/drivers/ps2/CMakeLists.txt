###############################################################################
# PS/2 Controller Driver
#
# Handles interfacing to the keyboard/mouse connected to the PS/2 controller
# on the motherboard.
###############################################################################
add_executable(ps2drv
    src/main.cpp
    src/Log.cpp
    src/Ps2Controller.cpp
    src/Ps2Command.cpp
    src/PortDetector.cpp
    src/SupportedDevices.cpp
    src/device/Keyboard.cpp
    src/device/GenericMouse.cpp
    src/device/ThreeAxisMouse.cpp
)

target_include_directories(ps2drv PRIVATE src/acpica/include)

# add platform specific code
if(${KERNEL_ARCH} STREQUAL "x86")
target_sources(ps2drv PRIVATE
    src/PortIo+x86.cpp
)
elseif(${KERNEL_ARCH} STREQUAL "x86_64")
target_sources(ps2drv PRIVATE
    src/PortIo+amd64.cpp
)
endif()

# allow link time optimization
target_compile_options(ps2drv PRIVATE -flto -fno-rtti -fno-exceptions)
target_link_options(ps2drv PRIVATE -s)

# search our codebase for includes
target_include_directories(ps2drv PRIVATE include)
target_include_directories(ps2drv PRIVATE src)

# required libraries
add_dependencies(ps2drv c_dynamic system_dynamic)
target_link_libraries(ps2drv PRIVATE capnp_dynamic driver rpc mpack_dynamic acpisrv_serialization)

# install it to boot directory in sysroot
install(TARGETS ps2drv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

