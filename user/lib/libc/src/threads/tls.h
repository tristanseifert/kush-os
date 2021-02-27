#ifndef LIBC_THREAD_TLS_H
#define LIBC_THREAD_TLS_H

#include <_libc.h>
#include <stddef.h>

LIBC_INTERNAL void __libc_tls_main_init();
LIBC_INTERNAL void __libc_tls_init();
LIBC_INTERNAL void __libc_tls_fini();

#endif
