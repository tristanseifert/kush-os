.global __ThreadTrampoline
.hidden __ThreadTrampoline
.type __ThreadTrampoline, function

__ThreadTrampoline:
    // clear registers
    xor         %rbp, %rbp

    // get destination pc...
    mov         -16(%rsp), %rbx
    // ...and the initial argument
    mov         -8(%rsp), %rdi

    // then jump to the destination routine
    call        *%rbx

    // exit thread
    call        Amd64ThreadExit

    // we should never get here!
    ud2

