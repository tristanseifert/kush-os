/**
 * Called in response to a kernel panic. Disable irqs and loops.
 *
 * We also try to send an IPI to all other cores to get them to stop doing what they're doing due
 * to the panic as well.
 */
.globl platform_panic_handler
platform_panic_handler:
    cli

.Lloop:
    hlt
    jmp         .Lloop
