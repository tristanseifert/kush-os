.global __ThreadTrampoline
.type __ThreadTrampoline, function

__ThreadTrampoline:
    # set up stack
    xor     %ebp, %ebp

    # read out the args
    mov     -4(%esp), %eax
    mov     -8(%esp), %ebx

    push    %eax
    call    *%ebx

    # destroy thread
    # TODO: this should be done from C
    #pushl   $0
    #call    ThreadDestroy
