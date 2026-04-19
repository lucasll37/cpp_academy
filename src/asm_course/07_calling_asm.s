# ============================================================
# src/asm_course/07_calling_asm.s
#
# MÓDULO 7 — Implementações ASM da Convenção de Chamada
# ============================================================

    .section .text

# ============================================================
# asm_soma6 — soma 6 argumentos inteiros
# Args: rdi, rsi, rdx, rcx, r8, r9
# ============================================================
    .globl asm_soma6
    .type  asm_soma6, @function
asm_soma6:
    # Todos os 6 argumentos estão em registradores
    # Simplesmente somamos:
    leaq    (%rdi,%rsi), %rax   # rax = a + b
    addq    %rdx, %rax          # rax += c
    addq    %rcx, %rax          # rax += d
    addq    %r8,  %rax          # rax += e
    addq    %r9,  %rax          # rax += f
    ret
    .size asm_soma6, .-asm_soma6


# ============================================================
# asm_soma8 — soma 8 argumentos (7° e 8° vêm da stack)
#
# Layout da stack ao entrar em asm_soma8:
#
#   [rsp+0]  = endereço de retorno   (empurrado pelo CALL)
#   [rsp+8]  = arg7                  (empurrado pelo caller)
#   [rsp+16] = arg8                  (empurrado pelo caller)
#
# Se fizermos o prólogo (push rbp; mov rbp,rsp):
#   [rbp+0]  = rbp anterior
#   [rbp+8]  = endereço de retorno
#   [rbp+16] = arg7
#   [rbp+24] = arg8
# ============================================================
    .globl asm_soma8
    .type  asm_soma8, @function
asm_soma8:
    pushq   %rbp
    movq    %rsp, %rbp

    # Soma os 6 args em registradores
    leaq    (%rdi,%rsi), %rax
    addq    %rdx, %rax
    addq    %rcx, %rax
    addq    %r8,  %rax
    addq    %r9,  %rax

    # Soma arg7 e arg8 da stack
    addq    16(%rbp), %rax      # arg7 = [rbp+16]
    addq    24(%rbp), %rax      # arg8 = [rbp+24]

    popq    %rbp
    ret
    .size asm_soma8, .-asm_soma8


# ============================================================
# asm_hypot — hipotenusa: sqrt(x² + y²) com SSE2
#
# Args: xmm0=x, xmm1=y
# Ret:  xmm0 = sqrt(x*x + y*y)
#
# Usa: MULSD (multiply scalar double)
#      ADDSD (add scalar double)
#      SQRTSD (sqrt scalar double)
# ============================================================
    .globl asm_hypot
    .type  asm_hypot, @function
asm_hypot:
    mulsd   %xmm0, %xmm0       # xmm0 = x²
    mulsd   %xmm1, %xmm1       # xmm1 = y²
    addsd   %xmm1, %xmm0       # xmm0 = x² + y²
    sqrtsd  %xmm0, %xmm0       # xmm0 = sqrt(x² + y²)
    ret
    .size asm_hypot, .-asm_hypot


# ============================================================
# asm_swap — troca dois inteiros de 64 bits via ponteiros
#
# Args: rdi=&a, rsi=&b
# Efeito: *a ↔ *b
# ============================================================
    .globl asm_swap
    .type  asm_swap, @function
asm_swap:
    movq    (%rdi), %rax        # rax = *a
    movq    (%rsi), %rcx        # rcx = *b
    movq    %rcx,  (%rdi)       # *a = rcx (= *b antigo)
    movq    %rax,  (%rsi)       # *b = rax (= *a antigo)
    ret
    .size asm_swap, .-asm_swap


# ============================================================
# asm_apply — chama um ponteiro de função com um argumento
#
# Args: rdi = ponteiro de função (int64_t(*)(int64_t))
#       rsi = argumento x
# Ret:  rax = fn(x)
#
# Indirect call: call *%rax  (chama o endereço em rax)
# ============================================================
    .globl asm_apply
    .type  asm_apply, @function
asm_apply:
    pushq   %rbp
    movq    %rsp, %rbp

    # ABI: caller deve garantir alinhamento de 16 bytes no CALL.
    # Após push rbp: rsp está misaligned por 8.
    # subq $8, %rsp alinha:  (rsp - 8) % 16 == 0
    subq    $8, %rsp

    movq    %rdi, %rax          # rax = endereço da função
    movq    %rsi, %rdi          # rdi = x (1° arg da função destino)
    call    *%rax               # chama fn(x) — resultado em rax

    addq    $8, %rsp
    popq    %rbp
    ret
    .size asm_apply, .-asm_apply


# ============================================================
# asm_print_variadic — chama printf com argumentos variádicos
#
# Demonstra o campo AL = número de xmm args usados.
# Em funções variádicas, o caller deve setar AL = número de
# registradores XMM com argumentos float (0-8).
# ============================================================
    .globl asm_print_variadic
    .type  asm_print_variadic, @function
asm_print_variadic:
    # rdi já tem o ponteiro da format string
    # AL deve conter número de xmm regs usados (0 aqui = sem floats)
    xorb    %al, %al            # al = 0 → sem argumentos SSE
    jmp     printf@plt          # tail call para printf
    .size asm_print_variadic, .-asm_print_variadic
