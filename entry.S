.global _start

_start:
    mov x0, sp

    ldr x1, [x0]        // argc
    add x2, x0, #8      // argv

    add x3, x2, x1, lsl #3
    add x3, x3, #8      // envp

    mov x0, x1
    mov x1, x2
    mov x2, x3

    bl main

    mov x8, #93
    svc #0