###################################################################################################
# Include the libpng CMake file, after changing some settings including removing some warning
# levels.
###################################################################################################
set(PNG_BUILD_ZLIB "" ON)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_STATIC OFF CACHE BOOL "" FORCE)

# do not install the libpng test programs, man pages, test programs
set(SKIP_INSTALL_PROGRAMS ON)
set(SKIP_INSTALL_EXECUTABLES ON)
set(SKIP_INSTALL_FILES ON)

# disable some warnings
add_compile_options(-Wno-implicit-fallthrough)

# ensure the default programs link properly
add_link_options(-lz -lopenlibm)

# finally, include the libpng CMake file
add_subdirectory(source)
