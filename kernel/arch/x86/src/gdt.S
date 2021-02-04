#define ASM_FILE
#include "gdt.h"

.global gdt_flush
gdt_flush:
    push        %eax
    mov         $GDT_KERN_DATA_SEG, %ax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs
    mov         %ax, %ss

    ljmp        $GDT_KERN_CODE_SEG, $flush_gdt_2

flush_gdt_2:
    pop         %eax
    ret

