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

/// Define this function as printf-like
#define KUSH_PRINTF_LIKE(fmt, args) __attribute__((format(printf,fmt,args)))
#else
#define KUSH_INLINE
#define KUSH_NOINLINE
#define KUSH_PACKED
#define KUSH_ALIGNED(x)
#define KUSH_PRINTF_LIKE(fmt, args)
#endif

#endif
