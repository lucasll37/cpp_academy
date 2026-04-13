// ============================================================
// src/inline_asm/main.cpp
//
// Assembly inline em C++ — guia progressivo
//
// Sintaxe usada: AT&T (padrão GCC/Clang no Linux x86-64)
//   - Operandos na ordem: src, dst  (oposto do Intel)
//   - Registradores prefixados com %  (%rax, %rbx …)
//   - Imediatos prefixados com $      ($42, $1 …)
//   - Sufixo de tamanho na instrução  (movq=64b, movl=32b …)
//
// Formato geral do asm inline GCC:
//
//   asm [volatile] (
//       "instruções"          // template de código
//       : outputs             // [opcional] operandos de saída
//       : inputs              // [opcional] operandos de entrada
//       : clobbers            // [opcional] registradores corrompidos
//   );
//
// ============================================================

#include <cstdint>
#include <fmt/core.h>

// ── separador visual ──────────────────────────────────────────
static void section(const char* title) {
    fmt::print("\n\033[1;36m══ {} ══\033[0m\n", title);
}

// ============================================================
// 1. ASM BÁSICO — instrução sem operandos C++
//    nop = No Operation. Não faz nada, mas é executada.
//    Útil para padding, delays, debugging.
// ============================================================
void demo_nop() {
    section("1. NOP — instrução sem operandos");

    asm("nop");   // forma curta: sem outputs, inputs ou clobbers

    fmt::print("  asm(\"nop\") executado — não faz nada visível\n");
    fmt::print("  mas o binário contém a instrução 0x90 (NOP)\n");
}

// ============================================================
// 2. OUTPUT — escrever resultado em variável C++
//
//   : "=r"(resultado)
//     ^   ^
//     |   variável C++ que receberá o valor
//     constraint:
//       = → operando de saída (write-only)
//       r → qualquer registrador de propósito geral
//
//   %0 no template → primeiro operando (resultado)
// ============================================================
void demo_output() {
    section("2. Output — asm escreve em variável C++");

    int resultado;

    asm(
        "movl $42, %0"      // move o imediato 42 para o operando 0
        : "=r"(resultado)   // operando 0 = resultado (output, qualquer reg)
    );

    fmt::print("  movl $42, %0  →  resultado = {}\n", resultado);

    // Com registrador explícito (eax)
    int valor_eax;
    asm(
        "movl $99, %%eax"   // %% → % literal dentro do template
        : "=a"(valor_eax)   // "a" → constraint para eax/ax/al
    );
    fmt::print("  movl $99, %%eax  →  valor_eax = {}\n", valor_eax);
}

// ============================================================
// 3. INPUT — ler variável C++ como operando
//
//   : "r"(x)
//     ^
//     sem = → operando de entrada (read-only)
//     r   → compilador escolhe qualquer registrador
//
//   %1 → segundo operando (primeiro input, após outputs)
// ============================================================
void demo_input() {
    section("3. Input — asm lê variável C++");

    int x = 7;
    int saida;

    asm(
        "movl %1, %0"       // copia o input (%1=x) para o output (%0=saida)
        : "=r"(saida)       // operando 0: output
        : "r"(x)            // operando 1: input  (x é lido, não escrito)
    );

    fmt::print("  x = {}  →  movl %1, %0  →  saida = {}\n", x, saida);
}

// ============================================================
// 4. INPUT + OUTPUT — operação aritmética
//
//   "+r"(n) → leitura E escrita do mesmo operando (read-write)
//             equivalente a: input e output usando o mesmo reg
// ============================================================
void demo_aritmetica() {
    section("4. Aritmética — add, sub, imul");

    // Adição: resultado = a + b
    int a = 10, b = 25, soma;
    asm(
        "movl %1, %0\n\t"   // %0 = a
        "addl %2, %0"       // %0 += b
        : "=r"(soma)
        : "r"(a), "r"(b)
    );
    fmt::print("  {} + {} = {}\n", a, b, soma);

    // Incremento in-place com "+r" (read-write)
    int n = 41;
    asm(
        "incl %0"           // %0 += 1
        : "+r"(n)           // + → lê e escreve o mesmo registrador
    );
    fmt::print("  incl: n = {} (era 41)\n", n);

    // Multiplicação: eax = eax * ecx
    int32_t fator = 6, multiplicando = 7, produto;
    asm(
        "imull %2, %0"      // %0 *= %2 (imul signed 32-bit)
        : "=r"(produto)
        : "0"(fator),       // "0" → usa o mesmo reg que o operando 0
          "r"(multiplicando)
    );
    fmt::print("  {} * {} = {}\n", fator, multiplicando, produto);
}

// ============================================================
// 5. CLOBBERS — avisar o compilador sobre registradores usados
//
//   Se o asm usa registradores que não estão em outputs/inputs,
//   devemos declará-los nos clobbers. Caso contrário, o
//   compilador pode ter valores importantes neles e perdê-los.
//
//   "cc"      → o asm pode alterar as flags (EFLAGS)
//   "memory"  → o asm pode ler/escrever memória arbitrária
//               (force o compilador a não reordenar acessos)
// ============================================================
void demo_clobbers() {
    section("5. Clobbers — declarando registradores usados");

    int x = 100, y = 200, troca_a, troca_b;

    // Troca dois valores via xchg (usa %rax e %rbx internamente)
    asm(
        "movl %2, %%eax\n\t"  // eax = x
        "movl %3, %%ebx\n\t"  // ebx = y
        "xchgl %%eax, %%ebx\n\t" // troca eax ↔ ebx
        "movl %%eax, %0\n\t"  // troca_a = eax (era y)
        "movl %%ebx, %1"      // troca_b = ebx (era x)
        : "=r"(troca_a), "=r"(troca_b)
        : "r"(x), "r"(y)
        : "%eax", "%ebx"      // ← clobbers: esses regs foram usados
    );

    fmt::print("  x={}, y={}  →  após xchg: troca_a={}, troca_b={}\n",
               x, y, troca_a, troca_b);

    // "cc" clobber: instrução que altera flags
    int val = 5;
    asm(
        "subl $3, %0"
        : "+r"(val)
        :
        : "cc"   // subl modifica EFLAGS (ZF, SF, CF…) → declarar "cc"
    );
    fmt::print("  subl $3: val = {} (era 5)\n", val);
}

// ============================================================
// 6. VOLATILE — impede otimizações indesejadas
//
//   Sem volatile, o compilador pode:
//     - Remover o asm se achar que o resultado não é usado
//     - Reordenar o asm com outras operações
//     - Executar o asm só uma vez se estiver em loop
//
//   volatile garante que o asm é executado exatamente onde
//   está, sem remoção ou reordenamento.
//   Outputs implica volatile. Sem outputs: sempre use volatile.
// ============================================================
void demo_volatile() {
    section("6. volatile — controle de reordenamento");

    // Sem outputs → DEVE ser volatile, senão o compilador remove
    asm volatile (
        "nop\n\t"   // instrução que o compilador não deve remover
        "nop\n\t"
        "nop"
    );
    fmt::print("  asm volatile com 3 NOPs — nunca removidos pelo otimizador\n");

    // Barreira de memória completa
    asm volatile ("" ::: "memory");
    fmt::print("  barreira de memória: impede reordenamento de loads/stores\n");
}

// ============================================================
// 7. MÚLTIPLAS INSTRUÇÕES + SALTOS
//
//   Cada instrução separada por \n\t (newline + tab)
//   Labels locais: %=  → gera número único por instância do asm
//   Permite loops e condicionais dentro do bloco asm.
// ============================================================
void demo_loop_asm() {
    section("7. Loop em assembly — soma 1..N");

    int N = 10, acumulador = 0;

    // Soma 1 + 2 + ... + N em assembly puro
    asm volatile (
        "xorl %%ecx, %%ecx\n\t"       // ecx = 0  (contador i)
        "xorl %%eax, %%eax\n\t"       // eax = 0  (acumulador)
        "loop_start%=:\n\t"            // label único (%= evita colisão)
        "  incl %%ecx\n\t"            // i++
        "  addl %%ecx, %%eax\n\t"     // acumulador += i
        "  cmpl %2, %%ecx\n\t"        // compara i com N
        "  jl   loop_start%=\n\t"     // se i < N, repete
        "movl %%eax, %0"              // saída: acumulador → resultado
        : "=r"(acumulador)            // output
        : "0"(acumulador), "r"(N)     // inputs
        : "%eax", "%ecx", "cc"        // clobbers
    );

    // Verifica: soma(1..10) = 55
    fmt::print("  soma(1..{}) = {}  (esperado: {})\n",
               N, acumulador, N*(N+1)/2);
}

// ============================================================
// 8. CPUID — instrução real de hardware
//
//   CPUID com eax=0 retorna:
//     eax: maior leaf suportada
//     ebx+ecx+edx: string do fabricante (12 chars)
//
//   Demonstra como chamar instruções que retornam múltiplos
//   registradores simultâneos.
// ============================================================
void demo_cpuid() {
    section("8. CPUID — lendo informações da CPU");

    uint32_t eax, ebx, ecx, edx;

    asm volatile (
        "cpuid"
        : "=a"(eax),   // output: eax → "a" constraint = registrador eax
          "=b"(ebx),   //         ebx → "b"
          "=c"(ecx),   //         ecx → "c"
          "=d"(edx)    //         edx → "d"
        : "a"(0)       // input:  eax=0 (leaf 0 = vendor string)
    );

    // Vendor string: 4 bytes de ebx + 4 de edx + 4 de ecx
    char vendor[13] = {};
    __builtin_memcpy(vendor + 0, &ebx, 4);
    __builtin_memcpy(vendor + 4, &edx, 4);
    __builtin_memcpy(vendor + 8, &ecx, 4);

    fmt::print("  CPUID leaf 0:\n");
    fmt::print("    eax (max leaf) = {}\n", eax);
    fmt::print("    vendor string  = \"{}\"\n", vendor);
    // GenuineIntel → Intel, AuthenticAMD → AMD
}

// ============================================================
// 9. RDTSC — lendo o contador de ciclos da CPU
//
//   RDTSC (Read Time-Stamp Counter) retorna o número de ciclos
//   desde o boot em edx:eax (32 bits cada).
//   Combinando: tsc = (edx << 32) | eax
//
//   Uso clássico: medir latência de código em ciclos.
// ============================================================
uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile (
        "rdtsc"
        : "=a"(lo),    // eax → parte baixa
          "=d"(hi)     // edx → parte alta
    );
    return ((uint64_t)hi << 32) | lo;
}

void demo_rdtsc() {
    section("9. RDTSC — contando ciclos de CPU");

    uint64_t t0 = rdtsc();

    // Trabalho arbitrário para medir
    volatile int soma = 0;
    for (int i = 0; i < 1000; ++i) soma += i;

    uint64_t t1 = rdtsc();

    fmt::print("  TSC antes : {}\n", t0);
    fmt::print("  TSC depois: {}\n", t1);
    fmt::print("  Ciclos para loop de 1000 somas: {}\n", t1 - t0);
}

// ============================================================
// 10. CONSTRAINTS DE MEMÓRIA — operando direto na memória
//
//   "m" → operando em memória (endereço de uma variável)
//   O compilador gera o endereçamento correto sem precisar
//   mover para um registrador primeiro.
// ============================================================
void demo_memory_constraint() {
    section("10. Constraint de memória — operando \"m\"");

    int arr[4] = {1, 2, 3, 4};
    int resultado;

    // Soma os 4 elementos usando constraint "m" para o array
    asm volatile (
        "movl  %1,    %%eax\n\t"   // eax = arr[0]
        "addl  4%1,   %%eax\n\t"   // eax += arr[1]  (offset +4 bytes)
        "addl  8%1,   %%eax\n\t"   // eax += arr[2]
        "addl  12%1,  %%eax\n\t"   // eax += arr[3]
        "movl  %%eax, %0"
        : "=r"(resultado)
        : "m"(*arr)               // "m" → compilador usa o endereço de arr
        : "%eax", "cc"
    );

    fmt::print("  arr = {{1,2,3,4}}  →  soma = {}\n", resultado);
}

// ============================================================
// 11. FLAGS E LEITURA DE CONDIÇÃO
//
//   Instrução TEST: AND bit a bit, afeta flags mas descarta resultado.
//   SETNE: escreve 1 em reg de 8 bits se ZF=0 (resultado ≠ 0)
//
//   Padrão clássico para converter resultado de instrução
//   que só afeta flags em um valor inteiro C++.
// ============================================================
void demo_flags() {
    section("11. Flags — test + setne");

    auto eh_par = [](int n) -> int {
        int resultado;
        asm (
            "testl $1, %1\n\t"    // n & 1 → afeta ZF (zero se par)
            "sete  %b0"           // %b0 = byte do operando 0
                                  // sete: byte = 1 se ZF=1 (par)
            : "=r"(resultado)
            : "r"(n)
            : "cc"
        );
        return resultado;
    };

    for (int n : {0, 1, 2, 3, 7, 8, 100}) {
        fmt::print("  {} é {}  (asm: {})\n",
                   n,
                   (n % 2 == 0) ? "par " : "ímpar",
                   eh_par(n) ? "par" : "ímpar");
    }
}

// ============================================================
// 12. NOMES SIMBÓLICOS — legibilidade em blocos grandes
//
//   Em vez de %0, %1, %2… use nomes explícitos:
//   [nome] "constraint"(variavel)
//   Referência no template: %[nome]
// ============================================================
void demo_named_operands() {
    section("12. Operandos nomeados — %[nome]");

    int largura = 16, altura = 9, area;

    asm (
        "movl %[w], %[out]\n\t"   // out = largura
        "imull %[h], %[out]"      // out *= altura
        : [out] "=r"(area)
        : [w]   "r"(largura),
          [h]   "r"(altura)
    );

    fmt::print("  {}×{} = {}  (via operandos nomeados)\n",
               largura, altura, area);
}

// ============================================================
int main() {
    fmt::print("\033[1;33m");
    fmt::print("╔══════════════════════════════════════════╗\n");
    fmt::print("║   Assembly Inline em C++ — cpp_academy   ║\n");
    fmt::print("║   x86-64 · AT&T syntax · GCC/Clang       ║\n");
    fmt::print("╚══════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_nop();
    demo_output();
    demo_input();
    demo_aritmetica();
    demo_clobbers();
    demo_volatile();
    demo_loop_asm();
    demo_cpuid();
    demo_rdtsc();
    demo_memory_constraint();
    demo_flags();
    demo_named_operands();

    fmt::print("\n\033[1;32m✓ Todas as demos executadas\033[0m\n\n");
    return 0;
}
