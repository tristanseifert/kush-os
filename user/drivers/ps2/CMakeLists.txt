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
    src/PortDetector.cpp
)

target_include_directories(ps2drv PRIVATE src/acpica/include)

# allow link time optimization
target_compile_options(ps2drv PRIVATE "-flto")

# search our codebase for includes
target_include_directories(ps2drv PRIVATE include)
target_include_directories(ps2drv PRIVATE src)

# required libraries
add_dependencies(ps2drv c_dynamic system_dynamic)
target_link_libraries(ps2drv PRIVATE rpc driver mpack_dynamic acpisrv_serialization)

# install it to boot directory in sysroot
install(TARGETS ps2drv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

