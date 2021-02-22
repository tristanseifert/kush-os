###############################################################################
# Userspace libraries
###############################################################################

# System libraries (these go in /lib)
add_subdirectory(libc)
add_subdirectory(libsystem)
add_subdirectory(librpc)

# third party libraries (most go in /usr/lib; some, like the C runtime, go in
# /lib like system libs)
add_subdirectory(external)
