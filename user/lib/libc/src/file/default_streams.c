#include <stdio.h>
#include <stddef.h>

/**
 * Define the default streams (stdin, stdout, and stderr) and provide the code to connect them at
 * library startup.
 */
FILE *__stdinp = NULL;
FILE *__stdoutp = NULL;
FILE *__stderrp = NULL;

/**
 * Connect the standard streams to the requisite consoles if they're set up. Otherwise, we create
 * null descriptors (basically the same as those pointing to /dev/null on UNIX-like systems) for
 * each of them.
 */
void __stdstream_init() {

}

