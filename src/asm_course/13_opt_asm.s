# ============================================================
# src/asm_course/13_opt_asm.s
# ============================================================
    .section .text

# Loop unrolled 4× para soma de array int32
    .globl asm_sum_unrolled
    .type  asm_sum_unrolled, @function
asm_sum_unrolled:
    xorq    %rax, %rax              # acumulador = 0
    movq    %rsi, %rcx
    shrq    $2,   %rcx              # rcx = n/4
    testq   %rcx, %rcx
    jz      .unroll_rem
.p2align 4
.unroll_loop:
    movslq   0(%rdi), %r8
    movslq   4(%rdi), %r9
    movslq   8(%rdi), %r10
    movslq  12(%rdi), %r11
    addq    %r8,  %rax
    addq    %r9,  %rax
    addq    %r10, %rax
    addq    %r11, %rax
    addq    $16,  %rdi              # avança 4 ints
    decq    %rcx
    jnz     .unroll_loop
.unroll_rem:
    andq    $3, %rsi
    jz      .unroll_done
.unroll_one:
    movslq  (%rdi), %r8
    addq    %r8, %rax
    addq    $4,  %rdi
    decq    %rsi
    jnz     .unroll_one
.unroll_done:
    ret
    .size asm_sum_unrolled, .-asm_sum_unrolled


# Soma SIMD com SSE2 (4 int32 por iteração via XMM)
    .globl asm_sum_simd
    .type  asm_sum_simd, @function
asm_sum_simd:
    pxor    %xmm0, %xmm0           # xmm0 = acumulador vetorial (4×int32)
    movq    %rsi,  %rcx
    shrq    $2,    %rcx             # rcx = n/4
    testq   %rcx,  %rcx
    jz      .simd_rem
.p2align 4
.simd_loop:
    movdqu  (%rdi), %xmm1          # carrega 4 int32 (unaligned ok)
    paddd   %xmm1,  %xmm0          # soma acumulada
    addq    $16, %rdi
    decq    %rcx
    jnz     .simd_loop
    # Redução horizontal: soma os 4 lanes de xmm0
    movdqa  %xmm0, %xmm1
    psrldq  $8,    %xmm1           # xmm1 = xmm0 >> 64 bits
    paddd   %xmm1, %xmm0
    movdqa  %xmm0, %xmm1
    psrldq  $4,    %xmm1
    paddd   %xmm1, %xmm0
    movd    %xmm0, %eax
    movslq  %eax,  %rax            # sign-extend para 64 bits
    jmp     .simd_done
.simd_rem:
    xorq    %rax, %rax
.simd_done:
    andq    $3, %rsi
    jz      .simd_ret
.simd_one:
    movslq  (%rdi), %r8
    addq    %r8, %rax
    addq    $4, %rdi
    decq    %rsi
    jnz     .simd_one
.simd_ret:
    ret
    .size asm_sum_simd, .-asm_sum_simd


# Cópia com prefetch antecipado
    .globl asm_prefetch_copy
    .type  asm_prefetch_copy, @function
asm_prefetch_copy:
    # rdi=src, rsi=dst, rdx=n  (int64_t)
    testq   %rdx, %rdx
    jz      .pfcopy_done
    movq    %rdx, %rcx
.p2align 4
.pfcopy_loop:
    prefetchnta 256(%rdi)           # pré-carrega 4 cache lines à frente
    movq    (%rdi), %rax
    movq    %rax, (%rsi)
    addq    $8, %rdi
    addq    $8, %rsi
    decq    %rcx
    jnz     .pfcopy_loop
.pfcopy_done:
    ret
    .size asm_prefetch_copy, .-asm_prefetch_copy
    .section .note.GNU-stack,"",@progbits
