###############################################################################
# Graphics driver support library
#
# Implements the protocol stubs for drivers that expose a graphics card, which
# in turn exposes one or more displays.
###############################################################################
add_library(libgfxdriver SHARED
    src/Serialization.cpp
    # display interface
    src/Server_Display.cpp
    src/Client_Display.cpp
    src/DisplayClient.cpp
)
set_target_properties(libgfxdriver PROPERTIES OUTPUT_NAME "DriverSupportGfxDriver")

target_include_directories(libgfxdriver PUBLIC include)
target_include_directories(libgfxdriver PRIVATE src)

target_compile_options(libgfxdriver PRIVATE -flto -fno-rtti -fno-exceptions)
target_link_libraries(libgfxdriver PUBLIC driver rpc mpack)

install(TARGETS libgfxdriver LIBRARY)

# export it under the DriverSupport space
add_library(DriverSupport::Graphics ALIAS libgfxdriver)
