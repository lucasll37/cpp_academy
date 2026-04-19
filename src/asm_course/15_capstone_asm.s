# ============================================================
# src/asm_course/15_capstone_asm.s
#
# MÓDULO 15 — Implementações de produção em Assembly x86-64
#
# Cada função segue a ABI System V AMD64 e é equivalente em
# comportamento às funções da libc (mas mais didáticas).
# ============================================================

    .section .text

# ============================================================
# asm_memcpy — cópia de memória com SIMD (128-bit XMM)
#
# void* asm_memcpy(void* dst, const void* src, size_t n);
#   rdi = dst, rsi = src, rdx = n
#   retorna rax = dst
#
# Estratégia:
#   1. Salva dst em rax (retorno)
#   2. Copia em blocos de 16 bytes via MOVUPS (unaligned)
#   3. Copia bytes restantes um a um
# ============================================================
    .globl asm_memcpy
    .type  asm_memcpy, @function
asm_memcpy:
    movq    %rdi, %rax          # rax = dst (retorno preservado)
    movq    %rdx, %rcx          # rcx = n

    # Blocos de 16 bytes (XMM)
    shrq    $4,   %rcx          # rcx = n / 16
    jz      .mcpy_tail
.p2align 4
.mcpy_xmm:
    movups  (%rsi), %xmm0       # carrega 16 bytes da fonte (unaligned)
    movups  %xmm0, (%rdi)       # escreve 16 bytes no destino (unaligned)
    addq    $16, %rsi
    addq    $16, %rdi
    decq    %rcx
    jnz     .mcpy_xmm

.mcpy_tail:
    andq    $15, %rdx           # rdx = n % 16 (bytes restantes)
    jz      .mcpy_done
.mcpy_byte:
    movb    (%rsi), %cl
    movb    %cl, (%rdi)
    incq    %rsi
    incq    %rdi
    decq    %rdx
    jnz     .mcpy_byte

.mcpy_done:
    ret                         # rax = dst (retorno)
    .size asm_memcpy, .-asm_memcpy


# ============================================================
# asm_memset — preenche memória com um byte
#
# void* asm_memset(void* dst, int c, size_t n);
#   rdi = dst, rsi = c (byte), rdx = n
#   retorna rax = dst
#
# Estratégia:
#   1. Transmite o byte para todos os 8 bytes de rax
#   2. Usa REP STOSQ para preencher em blocos de 8 bytes
#   3. Preenche bytes finais um a um
# ============================================================
    .globl asm_memset
    .type  asm_memset, @function
asm_memset:
    movq    %rdi, %r8           # r8 = dst (retorno)

    # Transmite byte para rax inteiro:
    # c = 0xAB → rax = 0xABABABABABABABAB
    movzbl  %sil, %eax          # eax = c (byte limpo)
    movabsq $0x0101010101010101, %rcx
    imulq   %rcx, %rax          # rax = c replicado em 8 bytes

    # REP STOSQ: escreve rax em [rdi], rdi += 8, rcx--; repete
    movq    %rdx, %rcx
    shrq    $3,   %rcx          # rcx = n / 8
    jz      .mset_tail
    rep     stosq               # preenche em blocos de 8 bytes

.mset_tail:
    andq    $7, %rdx            # rdx = n % 8
    jz      .mset_done
.mset_byte:
    movb    %al, (%rdi)         # al = byte de preenchimento
    incq    %rdi
    decq    %rdx
    jnz     .mset_byte

.mset_done:
    movq    %r8, %rax           # retorna dst
    ret
    .size asm_memset, .-asm_memset


# ============================================================
# asm_memcmp — compara duas regiões de memória
#
# int asm_memcmp(const void* a, const void* b, size_t n);
#   rdi = a, rsi = b, rdx = n
#   retorna: 0 se iguais, <0 se a<b, >0 se a>b
#
# Estratégia:
#   Compara byte a byte (simples e correto)
#   Retorna a diferença no primeiro byte diferente
# ============================================================
    .globl asm_memcmp
    .type  asm_memcmp, @function
asm_memcmp:
    testq   %rdx, %rdx
    jz      .mcmp_equal         # n=0 → sempre igual

.p2align 4
.mcmp_loop:
    movzbl  (%rdi), %eax        # eax = *a (zero-extended)
    movzbl  (%rsi), %ecx        # ecx = *b
    subl    %ecx,   %eax        # eax = *a - *b
    jnz     .mcmp_done          # diferença encontrada → retorna
    incq    %rdi
    incq    %rsi
    decq    %rdx
    jnz     .mcmp_loop

.mcmp_equal:
    xorl    %eax, %eax          # retorna 0

.mcmp_done:
    ret
    .size asm_memcmp, .-asm_memcmp


# ============================================================
# asm_strlen — comprimento de string
#
# size_t asm_strlen(const char* s);
#   rdi = s
#   retorna rax = comprimento (sem o '\0')
#
# Estratégia: REPNE SCASB busca '\0' decrement-contando
# ============================================================
    .globl asm_strlen
    .type  asm_strlen, @function
asm_strlen:
    movq    %rdi, %rcx          # rcx = ponteiro original
    xorb    %al, %al            # al = '\0' (byte buscado)
    movq    $-1, %rdx
    movq    %rdx, %rcx          # rcx = -1 (contador "infinito")
    repne   scasb               # busca '\0': rdi++, rcx-- até [rdi]==al
    notq    %rcx                # rcx = -(rcx+1) = nº de bytes varridos
    leaq    -2(%rcx), %rax      # -1 pelo '\0', -1 pelo ajuste do REPNE
    ret
    .size asm_strlen, .-asm_strlen


# ============================================================
# asm_isort — insertion sort para array de int32_t
#
# void asm_isort(int32_t* arr, int32_t n);
#   rdi = arr, rsi = n (int32)
#
# Algoritmo:
#   for i = 1..n-1:
#       key = arr[i]
#       j = i - 1
#       while j >= 0 && arr[j] > key:
#           arr[j+1] = arr[j]
#           j--
#       arr[j+1] = key
#
# Registradores:
#   rbx = i       (callee-saved)
#   r12 = arr     (callee-saved)
#   r13 = n       (callee-saved)
#   r14d = key    (callee-saved)
#   r15 = j       (callee-saved)
# ============================================================
    .globl asm_isort
    .type  asm_isort, @function
asm_isort:
    # ── Prólogo — preserva callee-saved ──────────────────────
    pushq   %rbp
    movq    %rsp, %rbp
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    movq    %rdi, %r12          # r12 = arr
    movslq  %esi, %r13          # r13 = n (sign-extend para 64b)

    cmpl    $2, %esi            # n < 2?
    jl      .isort_done         # nada a fazer

    movq    $1, %rbx            # i = 1

.isort_outer:
    cmpq    %r13, %rbx          # i >= n?
    jge     .isort_done

    # key = arr[i]
    movl    (%r12,%rbx,4), %r14d     # r14d = arr[i]  (key)

    # j = i - 1
    leaq    -1(%rbx), %r15           # r15 = j = i - 1

.isort_inner:
    testq   %r15, %r15               # j < 0?
    js      .isort_insert            # sim → sai do loop interno

    movl    (%r12,%r15,4), %eax      # eax = arr[j]
    cmpl    %r14d, %eax              # arr[j] > key?
    jle     .isort_insert            # não → para

    # arr[j+1] = arr[j]
    movl    %eax, 4(%r12,%r15,4)     # arr[j+1] = arr[j]
    decq    %r15                     # j--
    jmp     .isort_inner

.isort_insert:
    # arr[j+1] = key
    movl    %r14d, 4(%r12,%r15,4)    # arr[j+1] = key

    incq    %rbx                     # i++
    jmp     .isort_outer

.isort_done:
    # ── Epílogo ──────────────────────────────────────────────
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rbp
    ret
    .size asm_isort, .-asm_isort
