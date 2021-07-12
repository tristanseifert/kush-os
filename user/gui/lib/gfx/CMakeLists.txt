###################################################################################################
# libgfx: Graphics library containing low level drawing primitives.
#
# This is mostly just a wrapper around Cairo/Pixman.
###################################################################################################
add_library(GuiLibGfx SHARED
    # Cairo wrappers
    src/cairo/Context.cpp
    src/cairo/Pattern.cpp
    src/cairo/Surface.cpp
    src/cairo/Helpers.cpp
    # image loading
    src/image/Image.cpp
)

# link in the required libraries
target_link_libraries(GuiLibGfx PRIVATE cairo pixman)
target_link_libraries(GuiLibGfx PRIVATE freetype png)

set_target_properties(GuiLibGfx PROPERTIES OUTPUT_NAME "GuiGfx")

target_include_directories(GuiLibGfx PUBLIC include)
target_include_directories(GuiLibGfx PRIVATE src)

target_compile_options(GuiLibGfx PRIVATE -flto -fno-rtti -fno-exceptions)

install(TARGETS GuiLibGfx LIBRARY)

add_library(Gui::Graphics ALIAS GuiLibGfx)

