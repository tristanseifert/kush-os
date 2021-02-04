#ifndef _LIBALLOC_H
#define _LIBALLOC_H

#include <stddef.h>

//This lets you prefix malloc and friends
#define PREFIX(func)		k ## func

#ifdef __cplusplus
extern "C" {
#endif

extern void    *PREFIX(malloc)(size_t);				///< The standard function.
extern void    *PREFIX(realloc)(void *, size_t);		///< The standard function.
extern void    *PREFIX(calloc)(size_t, size_t);		///< The standard function.
extern void     PREFIX(free)(void *);					///< The standard function.


#ifdef __cplusplus
}
#endif


/** @} */

#endif


