/**
 * Called in response to a kernel panic. Disable irqs and loops.
 */
.globl platform_panic_handler
platform_panic_handler:
    cli

.Lloop:
    hlt
    jmp         .Lloop
