###############################################################################
# VMWare SVGA virtualized graphics adapter driver
#
# Provides a basic modesetting framebuffer driver on emulated VMWare SVGA
# devices. Eventually, this will support more meaningful forms of acceleration
# as well.
###############################################################################
add_executable(vmwaresvga
    src/main.cpp
    src/Log.cpp
    # basic GPU driver
    src/SVGA.cpp
    src/FIFO.cpp
    # 2D support
    src/Commands2D.cpp
)

target_compile_options(vmwaresvga PRIVATE -flto -fno-rtti -fno-exceptions)
target_link_options(vmwaresvga PRIVATE -s)

target_include_directories(vmwaresvga PRIVATE include)
target_include_directories(vmwaresvga PRIVATE src)

target_link_libraries(vmwaresvga PRIVATE driver rpc libpci)

install(TARGETS vmwaresvga RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

