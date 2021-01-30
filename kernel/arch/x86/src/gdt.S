.global gdt_flush
gdt_flush:
    push        %eax
    mov         $0x10, %ax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs
    mov         %ax, %ss

    ljmp        $0x08, $flush_gdt_2

flush_gdt_2:
    pop         %eax
    ret

