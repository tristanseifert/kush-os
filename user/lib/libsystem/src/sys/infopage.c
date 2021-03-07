#include "infopage.h"

/**
 * Define the info page. This is mapped in all tasks, so we assume it exists at this address as
 * part of the ABI contract with the system.
 *
 * The only caveat here is the root server: the info page isn't initialized until later on in its
 * startup, so we cannot rely on the page existing at startup unless we test for it.
 */
#if defined(__i386__)
const kush_sysinfo_page_t *__kush_infopg = (const kush_sysinfo_page_t *) 0xBF5FE000;
#elif defined(__amd64__)
const kush_sysinfo_page_t *__kush_infopg = (const kush_sysinfo_page_t *) 0x00007FFF00200000;

#else
#error Define sysinfo page address for current arch
#endif
