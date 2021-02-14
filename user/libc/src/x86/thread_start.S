.global __ThreadTrampoline

__ThreadTrampoline:
    # set up stack
    xor     %ebp, %ebp

    # read out the args
    mov     -4(%esp), %eax
    mov     -8(%esp), %ebx

    push    %eax
    call    *%ebx

    # destroy thread
    pushl   $0
    call    ThreadDestroy
