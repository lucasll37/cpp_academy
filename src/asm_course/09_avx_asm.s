# ============================================================
# src/asm_course/09_avx_asm.s
# ============================================================
    .section .text

    .globl asm_avx_add8f
    .type  asm_avx_add8f, @function
asm_avx_add8f:
    vmovaps (%rdi), %ymm0
    vmovaps (%rsi), %ymm1
    vaddps  %ymm1,  %ymm0, %ymm0
    vmovaps %ymm0, (%rdx)
    vzeroupper
    ret
    .size asm_avx_add8f, .-asm_avx_add8f

    .globl asm_avx_scale8f
    .type  asm_avx_scale8f, @function
asm_avx_scale8f:
    # rdi=arr, xmm0=scalar, esi=n
    vbroadcastss %xmm0, %ymm1      # ymm1 = [s,s,s,s,s,s,s,s]
    movl    %esi, %ecx
    shrl    $3,   %ecx             # ecx = n/8
    jz      .avx_scale_tail
.p2align 5
.avx_scale_loop:
    vmovups (%rdi), %ymm0
    vmulps  %ymm1,  %ymm0, %ymm0
    vmovups %ymm0, (%rdi)
    addq    $32, %rdi
    decl    %ecx
    jnz     .avx_scale_loop
.avx_scale_tail:
    andl    $7, %esi
    jz      .avx_scale_done
.avx_scale_one:
    vmovss  (%rdi), %xmm0
    vmulss  %xmm1,  %xmm0, %xmm0
    vmovss  %xmm0, (%rdi)
    addq    $4, %rdi
    decl    %esi
    jnz     .avx_scale_one
.avx_scale_done:
    vzeroupper
    ret
    .size asm_avx_scale8f, .-asm_avx_scale8f

    # FMA: out[i] = a[i]*b[i] + c[i]
    .globl asm_avx_fma
    .type  asm_avx_fma, @function
asm_avx_fma:
    # rdi=a, rsi=b, rdx=c, rcx=out, r8d=n
    movl    %r8d,  %r9d
    shrl    $3,    %r9d            # r9 = n/8
    jz      .fma_tail
.p2align 5
.fma_loop:
    vmovups (%rdi), %ymm0          # ymm0 = a[i..i+7]
    vmovups (%rsi), %ymm1          # ymm1 = b[i..i+7]
    vmovups (%rdx), %ymm2          # ymm2 = c[i..i+7]
    vfmadd231ps %ymm1, %ymm0, %ymm2  # ymm2 = ymm0*ymm1 + ymm2
    vmovups %ymm2, (%rcx)
    addq    $32, %rdi
    addq    $32, %rsi
    addq    $32, %rdx
    addq    $32, %rcx
    decl    %r9d
    jnz     .fma_loop
.fma_tail:
    andl    $7, %r8d
    jz      .fma_done
.fma_one:
    vmovss  (%rdi), %xmm0
    vmovss  (%rsi), %xmm1
    vmovss  (%rdx), %xmm2
    vfmadd231ss %xmm1, %xmm0, %xmm2
    vmovss  %xmm2, (%rcx)
    addq    $4, %rdi
    addq    $4, %rsi
    addq    $4, %rdx
    addq    $4, %rcx
    decl    %r8d
    jnz     .fma_one
.fma_done:
    vzeroupper
    ret
    .size asm_avx_fma, .-asm_avx_fma
