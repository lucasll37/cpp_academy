# inline_asm — Assembly Inline em C++ (x86-64, AT&T)

## O que é este projeto?

Guia progressivo de **assembly inline** em C++ usando a sintaxe AT&T (padrão do GCC/Clang no Linux). Cobre desde instruções básicas sem operandos até leitura de hardware (CPUID, RDTSC) e loops completos em assembly.

---

## Quando usar assembly inline?

- Acessar instruções de CPU sem equivalente em C++ (CPUID, RDTSC, SSE/AVX)
- Otimização extrema de hot loops onde o compilador não gera o código ideal
- Barreiras de memória mais precisas que `std::atomic`
- Código embarcado onde cada ciclo importa

Para a maioria dos casos, o compilador gera código tão bom quanto assembly manual — use com moderação.

---

## Estrutura de arquivos

```
inline_asm/
├── main.cpp   ← 12 demos progressivos
└── meson.build
```

---

## Sintaxe AT&T vs Intel

| Aspecto | AT&T (GCC/Linux) | Intel (NASM/Windows) |
|---|---|---|
| Ordem operandos | `src, dst` | `dst, src` |
| Registradores | `%rax`, `%rbx` | `rax`, `rbx` |
| Imediatos | `$42` | `42` |
| Sufixo de tamanho | `movl` (32b), `movq` (64b) | `mov` (com tipo no operando) |
| Desreferência | `(%rax)` | `[rax]` |

---

## Formato geral do `asm` inline GCC

```cpp
asm [volatile] (
    "instruções"          // template de código assembly
    : outputs             // [opcional] operandos de saída
    : inputs              // [opcional] operandos de entrada
    : clobbers            // [opcional] registradores que o asm modifica
);
```

---

## Demo 1: NOP — instrução sem operandos

```cpp
asm("nop");
// NOP = No Operation (opcode 0x90)
// O binário contém a instrução, mas não faz nada visível
// Usos: padding de alinhamento, delays em microcontroladores, debugging
```

---

## Demo 2: Output — asm escreve em variável C++

```cpp
int resultado;

asm(
    "movl $42, %0"      // move imediato 42 para o operando 0
    : "=r"(resultado)   // "=r" → output write-only, qualquer registrador
);
// resultado == 42

// Constraints de saída:
// "=r" → qualquer registrador de propósito geral (rax, rbx, rcx, etc.)
// "=a" → especificamente o registrador eax/ax/al
// "=m" → posição de memória (operando em memória)
```

---

## Demo 3: Input — asm lê variável C++

```cpp
int x = 7, saida;

asm(
    "movl %1, %0"    // copia input (%1=x) para output (%0=saida)
    : "=r"(saida)    // operando 0: output
    : "r"(x)         // operando 1: input (sem "=" → read-only)
);
// saida == 7

// %0 → primeiro operando (primeiro output)
// %1 → segundo operando (primeiro input)
// %2 → terceiro operando, etc.
```

---

## Demo 4: Aritmética — add, sub, imul

```cpp
// Adição
int a = 10, b = 25, soma;
asm(
    "movl %1, %0\n\t"  // %0 = a
    "addl %2, %0"      // %0 += b  →  a + b
    : "=r"(soma)
    : "r"(a), "r"(b)
);
// soma == 35

// Incremento in-place com "+r" (leitura E escrita)
int n = 41;
asm("incl %0" : "+r"(n));
// n == 42

// Multiplicação
int32_t fator = 6, mult = 7, produto;
asm(
    "imull %2, %0"
    : "=r"(produto)
    : "0"(fator),     // "0" → usa o mesmo registrador que operando 0
      "r"(mult)
);
// produto == 42
```

---

## Demo 5: Clobbers — declarando registradores usados

```cpp
// Se o asm usa registradores não declarados em outputs/inputs,
// deve declarar nos clobbers para o compilador não perder valores neles

int x = 100, y = 200, troca_a, troca_b;
asm(
    "movl %2, %%eax\n\t"
    "movl %3, %%ebx\n\t"
    "xchgl %%eax, %%ebx\n\t"
    "movl %%eax, %0\n\t"
    "movl %%ebx, %1"
    : "=r"(troca_a), "=r"(troca_b)
    : "r"(x), "r"(y)
    : "%eax", "%ebx"  // ← clobbers: esses regs foram usados internamente
);
// troca_a == 200, troca_b == 100

// Clobbers especiais:
// "cc"     → o asm modifica EFLAGS (resultado de comparações)
// "memory" → o asm pode ler/escrever memória arbitrária (barreira)
```

---

## Demo 6: `volatile` — prevenindo otimizações

```cpp
// Sem outputs → compilador pode REMOVER o asm (considera sem efeito)
// volatile garante que o asm é executado exatamente onde está

asm volatile ("nop\n\tnop\n\tnop");  // nunca removido pelo otimizador

// Barreira de memória completa:
asm volatile ("" ::: "memory");
// Impede reordenamento de loads/stores ao redor desta linha
// Equivalente mais forte que std::atomic_signal_fence(memory_order_seq_cst)
```

---

## Demo 7: Loop em assembly

```cpp
int N = 10, acumulador = 0;

// Soma 1 + 2 + ... + N em assembly puro
asm volatile (
    "xorl %%ecx, %%ecx\n\t"    // ecx = 0  (contador i)
    "xorl %%eax, %%eax\n\t"    // eax = 0  (acumulador)
    "loop_start%=:\n\t"         // %= gera label único por instância
    "  incl %%ecx\n\t"         // i++
    "  addl %%ecx, %%eax\n\t"  // acumulador += i
    "  cmpl %2, %%ecx\n\t"     // compara i com N
    "  jl   loop_start%=\n\t"  // se i < N → volta
    "movl %%eax, %0"
    : "=r"(acumulador)
    : "0"(acumulador), "r"(N)
    : "%eax", "%ecx", "cc"
);
// acumulador == 55  (10*11/2)
```

---

## Demo 8: CPUID — informações da CPU

```cpp
// CPUID com eax=0: retorna vendor string (12 chars em ebx+edx+ecx)
uint32_t eax, ebx, ecx, edx;
asm volatile (
    "cpuid"
    : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
    : "a"(0)  // leaf 0 = vendor info
);

char vendor[13] = {};
memcpy(vendor + 0, &ebx, 4);
memcpy(vendor + 4, &edx, 4);
memcpy(vendor + 8, &ecx, 4);
// "GenuineIntel" ou "AuthenticAMD"
fmt::print("CPU: {}\n", vendor);
```

---

## Demo 9: RDTSC — contando ciclos de CPU

```cpp
uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile (
        "rdtsc"
        : "=a"(lo),  // eax → 32 bits inferiores
          "=d"(hi)   // edx → 32 bits superiores
    );
    return ((uint64_t)hi << 32) | lo;
}

uint64_t t0 = rdtsc();
// ... código a medir ...
uint64_t t1 = rdtsc();
fmt::print("ciclos: {}\n", t1 - t0);
```

---

## Demo 10: Constraint de memória `"m"`

```cpp
int arr[4] = {1, 2, 3, 4};
int resultado;

asm volatile (
    "movl  %1,    %%eax\n\t"  // eax = arr[0]
    "addl  4%1,   %%eax\n\t"  // eax += arr[1]  (offset +4 bytes)
    "addl  8%1,   %%eax\n\t"  // eax += arr[2]
    "addl  12%1,  %%eax\n\t"  // eax += arr[3]
    "movl  %%eax, %0"
    : "=r"(resultado)
    : "m"(*arr)   // "m" → compilador gera o endereçamento correto
    : "%eax"
);
// resultado == 10
```

---

## Demo 11: Flags — TEST + SETE

```cpp
// Verifica se número é par usando TEST + SETE
auto eh_par = [](int n) -> int {
    int resultado;
    asm (
        "testl $1, %1\n\t" // n & 1 → afeta ZF (zero se par)
        "sete  %b0"         // byte do op 0 = 1 se ZF=1 (par)
        : "=r"(resultado)
        : "r"(n)
        : "cc"
    );
    return resultado;
};
// eh_par(8) == 1, eh_par(7) == 0
```

---

## Demo 12: Operandos nomeados

```cpp
// Em vez de %0, %1, %2... use nomes legíveis
int largura = 16, altura = 9, area;

asm (
    "movl %[w], %[out]\n\t"
    "imull %[h], %[out]"
    : [out] "=r"(area)
    : [w]   "r"(largura),
      [h]   "r"(altura)
);
// area == 144
```

---

## Tabela de constraints comuns

| Constraint | Significado |
|---|---|
| `"r"` | Qualquer registrador de propósito geral |
| `"a"` | Registrador `eax`/`rax` |
| `"b"` | Registrador `ebx`/`rbx` |
| `"c"` | Registrador `ecx`/`rcx` |
| `"d"` | Registrador `edx`/`rdx` |
| `"m"` | Operando em memória |
| `"i"` | Imediato inteiro |
| `"0"` | Mesmo registrador que operando 0 |
| `"="` | Write-only (output) |
| `"+"` | Read-write (input e output) |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja inline_asm
./bin/inline_asm
```

**Requer**: x86-64 Linux, GCC ou Clang.
