###################################################################################################
# Generic framebuffer console
###################################################################################################

add_library(platform-shared-fbcons STATIC EXCLUDE_FROM_ALL
    Sources/BitmapFonts.cpp
    Sources/Console.cpp
)
target_include_directories(platform-shared-fbcons PUBLIC ${CMAKE_CURRENT_LIST_DIR}/Includes)
target_include_directories(platform-shared-fbcons PRIVATE ${KERNEL_LIST_DIR}/Includes)
target_include_directories(platform-shared-fbcons PRIVATE ${KUSH_TOOLCHAIN_HEADERS})
target_compile_options(platform-shared-fbcons PRIVATE -fno-exceptions -fno-rtti -ffreestanding
    ${PLATFORM_SHARED_COMPILE_OPTS})

