# ============================================================
# src/asm_course/08_simd_asm.s
# ============================================================
    .section .text

    .globl asm_add_4floats
    .type  asm_add_4floats, @function
asm_add_4floats:
    # rdi=a, rsi=b, rdx=out
    movaps  (%rdi), %xmm0
    movaps  (%rsi), %xmm1
    addps   %xmm1,  %xmm0
    movaps  %xmm0, (%rdx)
    ret
    .size asm_add_4floats, .-asm_add_4floats

    .globl asm_dot4
    .type  asm_dot4, @function
asm_dot4:
    # rdi=a, rsi=b, rdx=*out
    movaps  (%rdi), %xmm0          # xmm0 = a
    movaps  (%rsi), %xmm1          # xmm1 = b
    mulps   %xmm1,  %xmm0          # xmm0 = a*b (packed)
    # Redução horizontal: soma todos os 4 floats
    movaps  %xmm0,  %xmm1
    shufps  $0b01001110, %xmm0, %xmm1  # xmm1 = [a2,a3,a0,a1]
    addps   %xmm1,  %xmm0          # xmm0 = [a0+a2, a1+a3, ...]
    movaps  %xmm0,  %xmm1
    shufps  $0b10110001, %xmm0, %xmm1  # xmm1 = [a1+a3, a0+a2, ...]
    addss   %xmm1,  %xmm0          # xmm0[0] = soma total
    movss   %xmm0, (%rdx)          # salva resultado
    ret
    .size asm_dot4, .-asm_dot4

    .globl asm_scale_array
    .type  asm_scale_array, @function
asm_scale_array:
    # rdi=arr, xmm0=scalar, esi=n
    # Transmite o escalar para todos os 4 lanes do xmm1
    shufps  $0, %xmm0, %xmm0       # xmm0 = [s,s,s,s]
    movl    %esi, %ecx
    shrl    $2,   %ecx              # ecx = n/4 (iterações de 4 em 4)
    testl   %ecx, %ecx
    jz      .scale_remainder
.scale_loop:
    movups  (%rdi), %xmm1           # load 4 floats (unaligned ok)
    mulps   %xmm0,  %xmm1           # multiplica por escalar
    movups  %xmm1, (%rdi)           # salva de volta
    addq    $16, %rdi               # avança 4 floats
    decl    %ecx
    jnz     .scale_loop
.scale_remainder:
    andl    $3, %esi                 # esi = n%4 elementos restantes
    jz      .scale_done
.scale_one:
    movss   (%rdi), %xmm1
    mulss   %xmm0,  %xmm1
    movss   %xmm1, (%rdi)
    addq    $4, %rdi
    decl    %esi
    jnz     .scale_one
.scale_done:
    ret
    .size asm_scale_array, .-asm_scale_array
