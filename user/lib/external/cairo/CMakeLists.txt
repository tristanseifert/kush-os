###################################################################################################
# Builds the cairo library.
#
# The same reasoning as with pixman applies here.
###################################################################################################
add_library(cairo SHARED
    source/src/cairo.c

    source/src/cairo-analysis-surface.c
    source/src/cairo-arc.c
    source/src/cairo-array.c
    source/src/cairo-atomic.c
    source/src/cairo-base64-stream.c
    source/src/cairo-base85-stream.c
    source/src/cairo-bentley-ottmann-rectangular.c
    source/src/cairo-bentley-ottmann-rectilinear.c
    source/src/cairo-bentley-ottmann.c
    source/src/cairo-botor-scan-converter.c
    source/src/cairo-boxes-intersect.c
    source/src/cairo-boxes.c
    source/src/cairo-cache.c
    source/src/cairo-cff-subset.c
    source/src/cairo-clip-boxes.c
    source/src/cairo-clip-polygon.c
    source/src/cairo-clip-region.c
    source/src/cairo-clip-surface.c
    source/src/cairo-clip-tor-scan-converter.c
    source/src/cairo-clip.c
    source/src/cairo-color.c
    source/src/cairo-composite-rectangles.c
    source/src/cairo-compositor.c
    source/src/cairo-contour.c
    source/src/cairo-damage.c
    source/src/cairo-debug.c
    source/src/cairo-default-context.c
    source/src/cairo-deflate-stream.c
    source/src/cairo-device.c
    source/src/cairo-error.c
    source/src/cairo-fallback-compositor.c
    source/src/cairo-fixed.c
    source/src/cairo-font-face-twin-data.c
    source/src/cairo-font-face-twin.c
    source/src/cairo-font-face.c
    source/src/cairo-font-options.c
    source/src/cairo-freed-pool.c
    source/src/cairo-freelist.c
    source/src/cairo-ft-font.c
    source/src/cairo-gstate.c
    source/src/cairo-hash.c
    source/src/cairo-hull.c
    source/src/cairo-image-compositor.c
    source/src/cairo-image-info.c
    source/src/cairo-image-source.c
    source/src/cairo-image-surface.c
    source/src/cairo-line.c
    source/src/cairo-lzw.c
    source/src/cairo-mask-compositor.c
    source/src/cairo-matrix.c
    source/src/cairo-mempool.c
    source/src/cairo-mesh-pattern-rasterizer.c
    source/src/cairo-misc.c
    source/src/cairo-mono-scan-converter.c
    source/src/cairo-mutex.c
    source/src/cairo-no-compositor.c
    source/src/cairo-observer.c
    source/src/cairo-output-stream.c
    source/src/cairo-paginated-surface.c
    source/src/cairo-path-bounds.c
    source/src/cairo-path-fill.c
    source/src/cairo-path-fixed.c
    source/src/cairo-path-in-fill.c
    source/src/cairo-path-stroke-boxes.c
    source/src/cairo-path-stroke-polygon.c
    source/src/cairo-path-stroke-traps.c
    source/src/cairo-path-stroke-tristrip.c
    source/src/cairo-path-stroke.c
    source/src/cairo-path.c
    source/src/cairo-pattern.c

source/src/cairo-pdf-interchange.c
source/src/cairo-pdf-operators.c
source/src/cairo-pdf-shading.c
source/src/cairo-pdf-surface.c
source/src/cairo-pen.c
source/src/cairo-png.c
source/src/cairo-polygon-intersect.c
source/src/cairo-polygon-reduce.c
source/src/cairo-polygon.c

source/src/cairo-raster-source-pattern.c
source/src/cairo-recording-surface.c
source/src/cairo-rectangle.c
source/src/cairo-rectangular-scan-converter.c
source/src/cairo-region.c
source/src/cairo-rtree.c
source/src/cairo-scaled-font-subsets.c
source/src/cairo-scaled-font.c
source/src/cairo-script-surface.c
source/src/cairo-shape-mask-compositor.c
source/src/cairo-slope.c
source/src/cairo-spans-compositor.c
source/src/cairo-spans.c
source/src/cairo-spline.c

source/src/cairo-stroke-dash.c
source/src/cairo-stroke-style.c
source/src/cairo-surface-clipper.c
source/src/cairo-surface-fallback.c
source/src/cairo-surface-observer.c
source/src/cairo-surface-offset.c
source/src/cairo-surface-snapshot.c
source/src/cairo-surface-subsurface.c
source/src/cairo-surface-wrapper.c
source/src/cairo-surface.c

source/src/cairo-tag-attributes.c
source/src/cairo-tag-stack.c
source/src/cairo-time.c
source/src/cairo-tor-scan-converter.c
source/src/cairo-tor22-scan-converter.c
source/src/cairo-toy-font-face.c
source/src/cairo-traps-compositor.c
source/src/cairo-traps.c
source/src/cairo-tristrip.c
source/src/cairo-truetype-subset.c
source/src/cairo-type1-fallback.c
source/src/cairo-type1-glyph-names.c
source/src/cairo-type1-subset.c
source/src/cairo-type3-glyph-surface.c
source/src/cairo-unicode.c
source/src/cairo-user-font.c
source/src/cairo-version.c
source/src/cairo-wideint.c
)

target_include_directories(cairo PRIVATE source/cairo)
target_link_options(cairo PRIVATE -flto)

# architecture specific stuff
if(${KERNEL_ARCH} STREQUAL "x86_64")
    # override the clang builtin provided long division functions; they may not be built with -fPIC
    # yet use the stack protector. since we keep the stack guard in libsystem, the relocations
    # output simply aren't large enough.
    target_sources(cairo PRIVATE
        extra/src/amd64/udivmodti4.c
        extra/src/amd64/umodti3.c
    )
endif()

# turn off some warnings
target_compile_options(cairo PRIVATE -Wno-ignored-attributes -Wno-cast-qual
    -Wno-parentheses-equality -Wno-enum-conversion -Wno-implicit-fallthrough -Wno-unused-function
    -Wno-unused-variable -Wno-unused-label -Wno-format-nonliteral -Wno-incompatible-pointer-types)
#target_compile_options(cairo PRIVATE -Wno-unused-local-typedef -Wno-cast-qual
#    -Wno-unused-const-variable -Wno-incompatible-function-pointer-types -Wno-implicit-fallthrough
#    -Wno-unused-function)

# emulate the includes generated by autoconf
target_compile_definitions(cairo PRIVATE HAVE_CONFIG_H)
target_include_directories(cairo PRIVATE extra/includes)

# we require pixman
target_link_libraries(cairo PRIVATE pixman z freetype)

# define public interface and install locations
target_include_directories(cairo PUBLIC include)
install(TARGETS cairo LIBRARY)
#set_target_properties(cairo PROPERTIES OUTPUT_NAME "pixman-1")

