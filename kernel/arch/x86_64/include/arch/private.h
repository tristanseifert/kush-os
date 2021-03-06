/*
 * Private helpers for headers
 */
#ifndef ARCH_X86_PRIVATE_H
#define ARCH_X86_PRIVATE_H

// macros for generating unique names
#define PP_CAT(a, b) PP_CAT_I(a, b)
#define PP_CAT_I(a, b) PP_CAT_II(~, a ## b)
#define PP_CAT_II(p, res) res
#define UNIQUE_NAME(base) PP_CAT(base, __COUNTER__)


#endif
