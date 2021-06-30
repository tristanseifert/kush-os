#define ASM_FILE
#include "gdt.h"
#include "exception_types.h"

.section .text


/// Generates an irq handler stub for the given irq number.
.macro GenerateIrqStub vec
.global pc64_irq_stub_\vec
.align 0x20
pc64_irq_stub_\vec:
    cli
    pushq       $\vec
    pushq       $X86_EXC_IRQ
    jmp         irq_common_stub
.endm

/// Declare all the vectors below... all 224 of them
GenerateIrqStub 32
GenerateIrqStub 33
GenerateIrqStub 34
GenerateIrqStub 35
GenerateIrqStub 36
GenerateIrqStub 37
GenerateIrqStub 38
GenerateIrqStub 39
GenerateIrqStub 40
GenerateIrqStub 41
GenerateIrqStub 42
GenerateIrqStub 43
GenerateIrqStub 44
GenerateIrqStub 45
GenerateIrqStub 46
GenerateIrqStub 47
GenerateIrqStub 48
GenerateIrqStub 49
GenerateIrqStub 50
GenerateIrqStub 51
GenerateIrqStub 52
GenerateIrqStub 53
GenerateIrqStub 54
GenerateIrqStub 55
GenerateIrqStub 56
GenerateIrqStub 57
GenerateIrqStub 58
GenerateIrqStub 59
GenerateIrqStub 60
GenerateIrqStub 61
GenerateIrqStub 62
GenerateIrqStub 63
GenerateIrqStub 64
GenerateIrqStub 65
GenerateIrqStub 66
GenerateIrqStub 67
GenerateIrqStub 68
GenerateIrqStub 69
GenerateIrqStub 70
GenerateIrqStub 71
GenerateIrqStub 72
GenerateIrqStub 73
GenerateIrqStub 74
GenerateIrqStub 75
GenerateIrqStub 76
GenerateIrqStub 77
GenerateIrqStub 78
GenerateIrqStub 79
GenerateIrqStub 80
GenerateIrqStub 81
GenerateIrqStub 82
GenerateIrqStub 83
GenerateIrqStub 84
GenerateIrqStub 85
GenerateIrqStub 86
GenerateIrqStub 87
GenerateIrqStub 88
GenerateIrqStub 89
GenerateIrqStub 90
GenerateIrqStub 91
GenerateIrqStub 92
GenerateIrqStub 93
GenerateIrqStub 94
GenerateIrqStub 95
GenerateIrqStub 96
GenerateIrqStub 97
GenerateIrqStub 98
GenerateIrqStub 99
GenerateIrqStub 100
GenerateIrqStub 101
GenerateIrqStub 102
GenerateIrqStub 103
GenerateIrqStub 104
GenerateIrqStub 105
GenerateIrqStub 106
GenerateIrqStub 107
GenerateIrqStub 108
GenerateIrqStub 109
GenerateIrqStub 110
GenerateIrqStub 111
GenerateIrqStub 112
GenerateIrqStub 113
GenerateIrqStub 114
GenerateIrqStub 115
GenerateIrqStub 116
GenerateIrqStub 117
GenerateIrqStub 118
GenerateIrqStub 119
GenerateIrqStub 120
GenerateIrqStub 121
GenerateIrqStub 122
GenerateIrqStub 123
GenerateIrqStub 124
GenerateIrqStub 125
GenerateIrqStub 126
GenerateIrqStub 127
GenerateIrqStub 128
GenerateIrqStub 129
GenerateIrqStub 130
GenerateIrqStub 131
GenerateIrqStub 132
GenerateIrqStub 133
GenerateIrqStub 134
GenerateIrqStub 135
GenerateIrqStub 136
GenerateIrqStub 137
GenerateIrqStub 138
GenerateIrqStub 139
GenerateIrqStub 140
GenerateIrqStub 141
GenerateIrqStub 142
GenerateIrqStub 143
GenerateIrqStub 144
GenerateIrqStub 145
GenerateIrqStub 146
GenerateIrqStub 147
GenerateIrqStub 148
GenerateIrqStub 149
GenerateIrqStub 150
GenerateIrqStub 151
GenerateIrqStub 152
GenerateIrqStub 153
GenerateIrqStub 154
GenerateIrqStub 155
GenerateIrqStub 156
GenerateIrqStub 157
GenerateIrqStub 158
GenerateIrqStub 159
GenerateIrqStub 160
GenerateIrqStub 161
GenerateIrqStub 162
GenerateIrqStub 163
GenerateIrqStub 164
GenerateIrqStub 165
GenerateIrqStub 166
GenerateIrqStub 167
GenerateIrqStub 168
GenerateIrqStub 169
GenerateIrqStub 170
GenerateIrqStub 171
GenerateIrqStub 172
GenerateIrqStub 173
GenerateIrqStub 174
GenerateIrqStub 175
GenerateIrqStub 176
GenerateIrqStub 177
GenerateIrqStub 178
GenerateIrqStub 179
GenerateIrqStub 180
GenerateIrqStub 181
GenerateIrqStub 182
GenerateIrqStub 183
GenerateIrqStub 184
GenerateIrqStub 185
GenerateIrqStub 186
GenerateIrqStub 187
GenerateIrqStub 188
GenerateIrqStub 189
GenerateIrqStub 190
GenerateIrqStub 191
GenerateIrqStub 192
GenerateIrqStub 193
GenerateIrqStub 194
GenerateIrqStub 195
GenerateIrqStub 196
GenerateIrqStub 197
GenerateIrqStub 198
GenerateIrqStub 199
GenerateIrqStub 200
GenerateIrqStub 201
GenerateIrqStub 202
GenerateIrqStub 203
GenerateIrqStub 204
GenerateIrqStub 205
GenerateIrqStub 206
GenerateIrqStub 207
GenerateIrqStub 208
GenerateIrqStub 209
GenerateIrqStub 210
GenerateIrqStub 211
GenerateIrqStub 212
GenerateIrqStub 213
GenerateIrqStub 214
GenerateIrqStub 215
GenerateIrqStub 216
GenerateIrqStub 217
GenerateIrqStub 218
GenerateIrqStub 219
GenerateIrqStub 220
GenerateIrqStub 221
GenerateIrqStub 222
GenerateIrqStub 223
GenerateIrqStub 224
GenerateIrqStub 225
GenerateIrqStub 226
GenerateIrqStub 227
GenerateIrqStub 228
GenerateIrqStub 229
GenerateIrqStub 230
GenerateIrqStub 231
GenerateIrqStub 232
GenerateIrqStub 233
GenerateIrqStub 234
GenerateIrqStub 235
GenerateIrqStub 236
GenerateIrqStub 237
GenerateIrqStub 238
GenerateIrqStub 239
GenerateIrqStub 240
GenerateIrqStub 241
GenerateIrqStub 242
GenerateIrqStub 243
GenerateIrqStub 244
GenerateIrqStub 245
GenerateIrqStub 246
GenerateIrqStub 247
GenerateIrqStub 248
GenerateIrqStub 249
GenerateIrqStub 250
GenerateIrqStub 251
GenerateIrqStub 252
GenerateIrqStub 253
GenerateIrqStub 254
GenerateIrqStub 255

/**
 * Common irq handler stub; this re-uses the same stack frame format as the exception frames. The
 * type value is set to X86_EXC_IRQ, and the error code is the vector number.
 */
.align 0x20
irq_common_stub:
    // save registers
    pushq        %rax
    pushq        %rcx
    pushq        %rdx
    pushq        %rbx
    pushq        %rbp
    pushq        %rsi
    pushq        %rdi
    pushq        %r8
    pushq        %r9
    pushq        %r10
    pushq        %r11
    pushq        %r12
    pushq        %r13
    pushq        %r14
    pushq        %r15

    // set up a NULL stack frame, so stack unwinding can stop here
    xor         %rbp, %rbp
    mov         %rsp, %rdi
    call        _Z14pc64_irq_entryP20amd64_exception_info

    // restore registers
    popq         %r15
    popq         %r14
    popq         %r13
    popq         %r12
    popq         %r11
    popq         %r10
    popq         %r9
    popq         %r8
    popq         %rdi
    popq         %rsi
    popq         %rbp
    popq         %rbx
    popq         %rdx
    popq         %rcx
    popq         %rax

    // clear the error code and interrupt number
    add         $0x10, %rsp

    // return; this will restore the interrupt flag too
    iretq

