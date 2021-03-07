.global __ThreadTrampoline
.type __ThreadTrampoline, function

__ThreadTrampoline:
    # set up stack
    xor         %rbp, %rbp

    # read out the args
    mov         -8(%rsp), %rax
    mov         -16(%rsp), %rbx

    mov         %rax, %rdi
    call        *%rbx

    # destroy thread
    # TODO: this should be done from C
    #pushl   $0
    #call    ThreadDestroy
