#define ASM_FILE
#include "gdt.h"

.global amd64_gdt_flush
amd64_gdt_flush:
    //mov         %rsp, %rbp

    /*
     * Set up a fake interrupt return stack frame so we can update the code segment. This is
     * necessary since amd64 removed the "far jump" opcode.
     */
    /*pushq       $GDT_KERN_DATA_SEG
    pushq       %rbp
    pushfq
    pop         %rax
    and         0b1111111111111111111111111111111111111111111111101011111011111111, %rax
    push        %rax*/
    pushq       $GDT_KERN_CODE_SEG
    pushq       $1f
    lretq
    //iretq

1:
    // fix the rest of the segments
    mov         $GDT_KERN_DATA_SEG, %ax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs
    mov         %ax, %ss

    // return
    ret


