# ============================================================
# src/outline_asm/asm_funcs.s
#
# Funções escritas inteiramente em assembly x86-64 (AT&T/GAS).
# Cada função segue o System V AMD64 ABI rigorosamente para
# poder ser chamada diretamente pelo C++ via extern "C".
#
# ── System V AMD64 ABI (resumo) ──────────────────────────────
#
#  Argumentos inteiros/ponteiros (em ordem):
#    1º → %rdi    4º → %rcx
#    2º → %rsi    5º → %r8
#    3º → %rdx    6º → %r9
#
#  Argumento float/double (em ordem):
#    1º → %xmm0   2º → %xmm1  … até %xmm7
#
#  Retorno:
#    inteiro / ponteiro → %rax
#    float / double     → %xmm0
#
#  Registradores CALLER-saved (pode usar livremente):
#    %rax, %rcx, %rdx, %rsi, %rdi, %r8–%r11, %xmm0–%xmm7
#
#  Registradores CALLEE-saved (DEVE preservar se usar):
#    %rbx, %rbp, %r12–%r15
#    → push no início, pop antes de ret
#
#  Stack:
#    - %rsp deve estar alinhado em 16 bytes no momento do call
#    - O call empurra 8 bytes (endereço de retorno) → antes de
#      qualquer push adicional, alinhe se necessário
#
# ── Diretivas GAS usadas ──────────────────────────────────────
#
#  .section .text    → código executável
#  .globl nome       → exporta o símbolo (visível ao linker)
#  .type nome, @function → informa ao linker que é função
#  .size nome, .-nome    → tamanho da função (para debug/profiling)
#
# ============================================================

    .section .text

# ============================================================
# 1. asm_soma_int — soma dois inteiros de 32 bits
#
#    Assinatura C++: int asm_soma_int(int a, int b);
#
#    Entrada:
#      %edi = a  (parte baixa de %rdi)
#      %esi = b  (parte baixa de %rsi)
#    Retorno:
#      %eax = a + b
# ============================================================
    .globl asm_soma_int
    .type  asm_soma_int, @function
asm_soma_int:
    movl    %edi, %eax      # eax = a
    addl    %esi, %eax      # eax += b   →   eax = a + b
    ret                     # retorna: caller lê %eax
    .size asm_soma_int, .-asm_soma_int


# ============================================================
# 2. asm_soma_int64 — soma dois inteiros de 64 bits
#
#    Assinatura C++: int64_t asm_soma_int64(int64_t a, int64_t b);
#
#    Entrada:
#      %rdi = a   (64 bits completos)
#      %rsi = b
#    Retorno:
#      %rax = a + b
# ============================================================
    .globl asm_soma_int64
    .type  asm_soma_int64, @function
asm_soma_int64:
    movq    %rdi, %rax      # rax = a
    addq    %rsi, %rax      # rax += b
    ret
    .size asm_soma_int64, .-asm_soma_int64


# ============================================================
# 3. asm_valor_absoluto — |n| para inteiro de 32 bits
#
#    Assinatura C++: int asm_valor_absoluto(int n);
#
#    Técnica: aritmética de complemento de 2 sem branch.
#      mask = n >> 31   →  0x00000000 se n>=0, 0xFFFFFFFF se n<0
#      |n|  = (n XOR mask) - mask
#
#    Entrada:  %edi = n
#    Retorno:  %eax = |n|
# ============================================================
    .globl asm_valor_absoluto
    .type  asm_valor_absoluto, @function
asm_valor_absoluto:
    movl    %edi, %eax      # eax = n
    movl    %edi, %edx      # edx = n
    sarl    $31,  %edx      # edx = n >> 31 (shift aritmético → propaga bit de sinal)
                            # edx = 0x00000000 se n >= 0
                            # edx = 0xFFFFFFFF se n < 0  (= -1 em complemento 2)
    xorl    %edx, %eax      # eax = n XOR mask
                            # se n>=0: eax XOR 0 = n (inalterado)
                            # se n< 0: eax XOR 0xFF...FF = ~n
    subl    %edx, %eax      # eax -= mask
                            # se n>=0: n - 0   = n
                            # se n< 0: ~n - (-1) = ~n + 1 = -n  ✓
    ret
    .size asm_valor_absoluto, .-asm_valor_absoluto


# ============================================================
# 4. asm_fatorial — fatorial iterativo (n! para n <= 20)
#
#    Assinatura C++: int64_t asm_fatorial(int n);
#
#    Entrada:  %edi = n
#    Retorno:  %rax = n!
#
#    Usa %rcx como contador — é caller-saved, não precisa preservar.
# ============================================================
    .globl asm_fatorial
    .type  asm_fatorial, @function
asm_fatorial:
    movl    %edi,   %ecx    # ecx = n  (contador)
    movq    $1,     %rax    # rax = 1  (acumulador)
    testl   %ecx,   %ecx    # n == 0?
    jle     .fat_fim        # se n <= 0, retorna 1
.fat_loop:
    imulq   %rcx,   %rax    # rax *= rcx
    decl    %ecx            # ecx--
    jnz     .fat_loop       # se ecx != 0, continua
.fat_fim:
    ret
    .size asm_fatorial, .-asm_fatorial


# ============================================================
# 5. asm_maximo — retorna o maior de dois inteiros (sem branch)
#
#    Assinatura C++: int asm_maximo(int a, int b);
#
#    Técnica branchless com CMOVGE (conditional move):
#      resultado = a
#      se b > a: resultado = b
#
#    Entrada:  %edi = a,  %esi = b
#    Retorno:  %eax = max(a, b)
# ============================================================
    .globl asm_maximo
    .type  asm_maximo, @function
asm_maximo:
    movl    %edi, %eax      # eax = a  (resultado inicial)
    cmpl    %esi, %edi      # compara a com b (afeta flags, não escreve)
    cmovll  %esi, %eax      # se a < b (less): eax = b
                            # cmov não cria branch → pipeline não quebra
    ret
    .size asm_maximo, .-asm_maximo


# ============================================================
# 6. asm_popcount — conta bits 1 em um inteiro de 32 bits
#
#    Assinatura C++: int asm_popcount(uint32_t x);
#
#    Usa a instrução POPCNT introduzida no SSE4.2.
#    Entrada:  %edi = x
#    Retorno:  %eax = número de bits 1
# ============================================================
    .globl asm_popcount
    .type  asm_popcount, @function
asm_popcount:
    popcntl %edi, %eax      # eax = __builtin_popcount(edi)
    ret
    .size asm_popcount, .-asm_popcount


# ============================================================
# 7. asm_strlen — comprimento de string (como strlen da libc)
#
#    Assinatura C++: int64_t asm_strlen(const char* s);
#
#    Entrada:  %rdi = ponteiro para a string
#    Retorno:  %rax = número de caracteres (sem o '\0')
#
#    Usa SCASB: compara AL com byte em [RDI] e incrementa RDI.
#    REPNE SCASB: repete enquanto byte != AL (procura '\0').
# ============================================================
    .globl asm_strlen
    .type  asm_strlen, @function
asm_strlen:
    movq    %rdi,   %rcx    # rcx = ponteiro original (salva para calcular len)
    xorb    %al,    %al     # al = 0  (byte que procuramos: '\0')
    movq    $-1,    %rdx    # rdx = -1 (contador máximo: busca "infinita")
    movq    %rdx,   %rcx    # rcx = -1 (REPNE decrementa rcx a cada byte)
    repne   scasb           # repete: if [rdi] == al → para; else rdi++, rcx--
                            # ao terminar: rdi aponta para o byte APÓS o '\0'
    notq    %rcx            # rcx = ~rcx = número de bytes varridos
    leaq    -2(%rcx), %rax  # rax = rcx - 2
                            # -1: o '\0' foi contado, -1: ajuste do REPNE
    ret
    .size asm_strlen, .-asm_strlen


# ============================================================
# 8. asm_memcpy_64 — copia N blocos de 64 bits (8 bytes cada)
#
#    Assinatura C++: void asm_memcpy_64(void* dst, const void* src, int64_t n);
#
#    Entrada:
#      %rdi = dst   (destino)
#      %rsi = src   (fonte)
#      %rdx = n     (número de qwords = blocos de 8 bytes)
#
#    REP MOVSQ: copia %rcx qwords de [RSI] para [RDI],
#               incrementando RSI e RDI automaticamente.
# ============================================================
    .globl asm_memcpy_64
    .type  asm_memcpy_64, @function
asm_memcpy_64:
    movq    %rdx, %rcx      # rcx = n  (REP usa rcx como contador)
    rep     movsq           # copia rcx qwords: [rsi] → [rdi], rsi++, rdi++
    ret
    .size asm_memcpy_64, .-asm_memcpy_64


# ============================================================
# 9. asm_soma_array — soma todos os elementos de um array int32
#
#    Assinatura C++: int64_t asm_soma_array(const int* arr, int n);
#
#    Entrada:
#      %rdi = ponteiro para o array
#      %esi = n (número de elementos)
#    Retorno:
#      %rax = soma acumulada (int64 para evitar overflow)
# ============================================================
    .globl asm_soma_array
    .type  asm_soma_array, @function
asm_soma_array:
    xorq    %rax, %rax      # rax = 0  (acumulador)
    testl   %esi, %esi      # n == 0?
    jle     .arr_fim        # se n <= 0, retorna 0
    movl    %esi, %ecx      # ecx = n  (contador)
.arr_loop:
    movslq  (%rdi), %rdx    # rdx = *arr  (sign-extend int32 → int64)
    addq    %rdx,   %rax    # rax += *arr
    addq    $4,     %rdi    # arr++  (4 bytes por int32)
    decl    %ecx            # n--
    jnz     .arr_loop       # se n != 0, continua
.arr_fim:
    ret
    .size asm_soma_array, .-asm_soma_array


# ============================================================
# 10. asm_eh_primo — testa se n é primo (trial division)
#
#     Assinatura C++: int asm_eh_primo(int n);
#
#     Retorno: 1 se primo, 0 caso contrário.
#
#     Usa %rbx (callee-saved) → deve preservar com push/pop.
#     Demonstra o padrão correto de preservação de registradores.
# ============================================================
    .globl asm_eh_primo
    .type  asm_eh_primo, @function
asm_eh_primo:
    pushq   %rbx                # preserva rbx (callee-saved)

    movl    %edi,   %eax        # eax = n
    cmpl    $2,     %eax        # n < 2?
    jl      .primo_nao          # não é primo
    cmpl    $2,     %eax
    je      .primo_sim          # n == 2 → primo

    testl   $1,     %eax        # n par?
    jz      .primo_nao          # par e > 2 → não primo

    movl    $3,     %ebx        # ebx = divisor = 3
.primo_loop:
    movl    %ebx,   %eax        # eax = divisor
    imull   %ebx,   %eax        # eax = divisor²
    cmpl    %edi,   %eax        # divisor² > n?
    jg      .primo_sim          # sim → é primo

    movl    %edi,   %eax        # eax = n
    xorl    %edx,   %edx        # edx = 0  (parte alta do dividendo)
    divl    %ebx                # eax = n / divisor,  edx = n % divisor
    testl   %edx,   %edx        # resto == 0?
    jz      .primo_nao          # divisível → não primo

    addl    $2,     %ebx        # divisor += 2 (só ímpares)
    jmp     .primo_loop

.primo_sim:
    movl    $1, %eax
    jmp     .primo_ret
.primo_nao:
    xorl    %eax, %eax
.primo_ret:
    popq    %rbx                # restaura rbx
    ret
    .size asm_eh_primo, .-asm_eh_primo
