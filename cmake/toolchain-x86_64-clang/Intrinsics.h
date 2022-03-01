/*
 * Define various preprocessor shortcuts for compiler-specific attributes, for clang.
 */
#ifndef TOOLCHAIN_X86_64_CLANG_INTRINSICS_H
#define TOOLCHAIN_X86_64_CLANG_INTRINSICS_H

// do _not_ declare these attributes for doxygen (it gets confused)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
/// Function should be inlined
#define KUSH_INLINE __attribute__((inline))
/// Function should never be inlined
#define KUSH_NOINLINE __attribute__((noinline))
/// Pack elements in the structure tightly
#define KUSH_PACKED __attribute__((packed))
/// Align the data with the given alignment
#define KUSH_ALIGNED(x) __attribute__((aligned(x)))
#else
#define KUSH_INLINE
#define KUSH_NOINLINE
#define KUSH_PACKED
#endif

#endif
