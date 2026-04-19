# ============================================================
# src/asm_course/12_x87_asm.s
# ============================================================
    .section .text

    .globl asm_x87_sqrt
    .type  asm_x87_sqrt, @function
asm_x87_sqrt:
    # xmm0=x (double) → resultado via x87 → xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)      # salva double na stack
    fldl    (%rsp)              # carrega na pilha x87: ST(0) = x
    fsqrt                       # ST(0) = sqrt(ST(0))
    fstpl   (%rsp)              # pop para stack
    movsd   (%rsp), %xmm0      # retorna em xmm0
    addq    $8, %rsp
    ret
    .size asm_x87_sqrt, .-asm_x87_sqrt

    .globl asm_x87_sin
    .type  asm_x87_sin, @function
asm_x87_sin:
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    fldl    (%rsp)
    fsin                        # ST(0) = sin(ST(0))
    fstpl   (%rsp)
    movsd   (%rsp), %xmm0
    addq    $8, %rsp
    ret
    .size asm_x87_sin, .-asm_x87_sin

    .globl asm_x87_atan2
    .type  asm_x87_atan2, @function
asm_x87_atan2:
    # xmm0=y, xmm1=x → atan2(y,x) = atan(y/x) com sinal correto
    subq    $16, %rsp
    movsd   %xmm0,  (%rsp)     # salva y
    movsd   %xmm1, 8(%rsp)     # salva x
    fldl    8(%rsp)             # ST(0) = x
    fldl     (%rsp)             # ST(0) = y, ST(1) = x
    fpatan                      # ST(0) = atan(ST(1)/ST(0)) com sinais
    fstpl   (%rsp)
    movsd   (%rsp), %xmm0
    addq    $16, %rsp
    ret
    .size asm_x87_atan2, .-asm_x87_atan2

    .globl asm_x87_log2
    .type  asm_x87_log2, @function
asm_x87_log2:
    # log2(x) via FYL2X: ST(1)*log2(ST(0)), pop
    # FYL2X: ST(1) = ST(1) * log2(ST(0)); pop ST(0)
    # Para log2(x): empilha 1.0, depois x; FYL2X → 1.0 * log2(x)
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    fld1                        # ST(0) = 1.0
    fldl    (%rsp)              # ST(0) = x, ST(1) = 1.0
    fyl2x                       # ST(0) = 1.0 * log2(x), pop
    fstpl   (%rsp)
    movsd   (%rsp), %xmm0
    addq    $8, %rsp
    ret
    .size asm_x87_log2, .-asm_x87_log2
