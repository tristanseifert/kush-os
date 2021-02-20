#ifndef _LIBC_NLTYPES_H_
#define	_LIBC_NLTYPES_H_

#include <sys/types.h>

#define    NL_SETD        0
#define    NL_CAT_LOCALE    1

typedef struct __nl_cat_d {
    void    *__data;
    int    __size;
} *nl_catd;

#ifndef _NL_ITEM_DECLARED
typedef    int    nl_item;
#define    _NL_ITEM_DECLARED
#endif


#ifdef __cplusplus
extern "C" {
#endif
nl_catd  catopen(const char *, int);
char    *catgets(nl_catd, int, int, const char *);
int     catclose(nl_catd);
#ifdef __cplusplus
}
#endif


#endif
