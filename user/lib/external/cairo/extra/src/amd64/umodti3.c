/*
 * This file was copied from llvm's compiler-rt library.
 */
#include "types.h"

#include <limits.h>
#include <stddef.h>
#include <sys/types.h>

extern tu_int __udivmodti4(tu_int a, tu_int b, tu_int *rem);

// Returns: a % b
tu_int __umodti3(tu_int a, tu_int b) {
  tu_int r;
  __udivmodti4(a, b, &r);
  return r;
}

