# ============================================================
# src/asm_course/06_stack_asm.s
#
# MÓDULO 6 — Implementações ASM de Stack Frames
#
# Demonstra:
#   - Prólogo/epílogo canônico
#   - Variáveis locais no frame (sub rsp, N)
#   - Preservação de callee-saved registers (rbx, r12-r15)
#   - Recursão com frame correto
#   - Inspeção do frame em runtime
# ============================================================

    .section .text

# ============================================================
# asm_inspect_frame — captura RSP, RBP e endereço de retorno
#
# Assinatura C++:
#   void asm_inspect_frame(uint64_t* out_rsp, uint64_t* out_rbp,
#                          uint64_t* out_ret_addr);
#
# Argumentos:
#   %rdi = out_rsp       (ponteiro para onde salvar RSP)
#   %rsi = out_rbp       (ponteiro para onde salvar RBP)
#   %rdx = out_ret_addr  (ponteiro para onde salvar [RBP+8])
#
# Depois do prólogo (push rbp; mov rbp,rsp):
#   [rbp+0] = RBP antigo (salvo pelo push)
#   [rbp+8] = endereço de retorno (empurrado pelo CALL)
# ============================================================
    .globl asm_inspect_frame
    .type  asm_inspect_frame, @function
asm_inspect_frame:
    # ── Prólogo ──────────────────────────────────────────────
    pushq   %rbp
    movq    %rsp, %rbp

    # Agora o frame está estabelecido:
    #   [rbp+0] = rbp anterior
    #   [rbp+8] = endereço de retorno (empurrado pelo CALL do C++)
    #   rbp     = base do frame atual
    #   rsp     = rbp (nenhuma variável local alocada)

    # Salva RSP atual (= RBP pois não alocamos nada abaixo)
    movq    %rsp, (%rdi)        # *out_rsp = rsp

    # Salva RBP atual
    movq    %rbp, (%rsi)        # *out_rbp = rbp

    # Lê o endereço de retorno em [RBP+8]
    movq    8(%rbp), %rax       # rax = endereço de retorno
    movq    %rax, (%rdx)        # *out_ret_addr = rax

    # ── Epílogo ──────────────────────────────────────────────
    popq    %rbp                # restaura rbp anterior
    ret                         # pop rip → volta ao caller
    .size asm_inspect_frame, .-asm_inspect_frame


# ============================================================
# asm_with_locals — demonstra variáveis locais no stack frame
#
# Assinatura C++:
#   int64_t asm_with_locals(int64_t a, int64_t b, int64_t c);
#
# Argumentos: rdi=a, rsi=b, rdx=c
# Retorno:    rax = (a*b) + (b*c) + (a+c)
#
# Variáveis locais criadas no frame:
#   -8(%rbp)  = tmp1 = a * b
#   -16(%rbp) = tmp2 = b * c
#   -24(%rbp) = tmp3 = a + c
# ============================================================
    .globl asm_with_locals
    .type  asm_with_locals, @function
asm_with_locals:
    # ── Prólogo com alocação de variáveis locais ─────────────
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $24, %rsp           # aloca 24 bytes para 3 variáveis int64

    # Agora o frame:
    #   [rbp+ 0] = rbp anterior
    #   [rbp+ 8] = endereço de retorno
    #   [rbp- 8] = tmp1  ← nossa var local
    #   [rbp-16] = tmp2  ← nossa var local
    #   [rbp-24] = tmp3  ← nossa var local
    #   rsp      = rbp - 24

    # ── Corpo da função ──────────────────────────────────────

    # tmp1 = a * b
    movq    %rdi, %rax          # rax = a
    imulq   %rsi, %rax          # rax = a * b
    movq    %rax, -8(%rbp)      # salva tmp1 no frame

    # tmp2 = b * c
    movq    %rsi, %rax          # rax = b
    imulq   %rdx, %rax          # rax = b * c
    movq    %rax, -16(%rbp)     # salva tmp2 no frame

    # tmp3 = a + c
    movq    %rdi, %rax          # rax = a
    addq    %rdx, %rax          # rax = a + c
    movq    %rax, -24(%rbp)     # salva tmp3 no frame

    # resultado = tmp1 + tmp2 + tmp3
    movq    -8(%rbp),  %rax     # rax = tmp1
    addq    -16(%rbp), %rax     # rax += tmp2
    addq    -24(%rbp), %rax     # rax += tmp3

    # ── Epílogo ──────────────────────────────────────────────
    leave                       # equivale a: mov rsp,rbp; pop rbp
    ret
    .size asm_with_locals, .-asm_with_locals


# ============================================================
# asm_fibonacci — fibonacci recursivo em assembly
#
# Assinatura C++: int64_t asm_fibonacci(int64_t n);
#
# Argumento: rdi = n
# Retorno:   rax = fib(n)
#
# fib(0) = 0
# fib(1) = 1
# fib(n) = fib(n-1) + fib(n-2)
#
# Registradores callee-saved usados:
#   %rbx = salva n entre as chamadas recursivas
#   %r12 = salva o resultado de fib(n-1)
#
# Por que precisamos salvar rbx e r12?
#   Porque a chamada recursiva vai usar rax/rcx/rdx/rsi/rdi/r8-r11
#   livremente (são caller-saved). Mas rbx/r12-r15 são callee-saved:
#   SE UMA FUNÇÃO OS USA, ela DEVE restaurá-los ao retornar.
# ============================================================
    .globl asm_fibonacci
    .type  asm_fibonacci, @function
asm_fibonacci:
    # ── Prólogo — salva callee-saved registers ───────────────
    pushq   %rbp
    movq    %rsp, %rbp
    pushq   %rbx                # rbx é callee-saved → devemos salvar
    pushq   %r12                # r12 é callee-saved → devemos salvar

    # Agora o frame:
    #   [rbp+ 0] = rbp anterior
    #   [rbp+ 8] = endereço de retorno
    #   [rbp- 8] = rbx salvo
    #   [rbp-16] = r12 salvo
    #   rsp      = rbp - 16

    # ── Casos base ───────────────────────────────────────────
    movq    %rdi, %rbx          # rbx = n  (salva n para uso posterior)

    cmpq    $0, %rbx
    je      .fib_ret_zero       # fib(0) = 0

    cmpq    $1, %rbx
    je      .fib_ret_one        # fib(1) = 1

    # ── Caso recursivo: fib(n) = fib(n-1) + fib(n-2) ────────

    # Calcula fib(n-1)
    leaq    -1(%rbx), %rdi      # rdi = n - 1
    call    asm_fibonacci        # rax = fib(n-1)
    movq    %rax, %r12          # r12 = fib(n-1)  (salva antes da próx. chamada)

    # Calcula fib(n-2)
    leaq    -2(%rbx), %rdi      # rdi = n - 2  (rbx ainda tem n)
    call    asm_fibonacci        # rax = fib(n-2)

    # resultado = fib(n-1) + fib(n-2)
    addq    %r12, %rax          # rax = fib(n-1) + fib(n-2)

    # ── Epílogo ──────────────────────────────────────────────
    jmp     .fib_done

.fib_ret_zero:
    xorq    %rax, %rax          # rax = 0
    jmp     .fib_done

.fib_ret_one:
    movq    $1, %rax            # rax = 1

.fib_done:
    popq    %r12                # restaura r12 (inverso do push!)
    popq    %rbx                # restaura rbx
    popq    %rbp
    ret
    .size asm_fibonacci, .-asm_fibonacci
