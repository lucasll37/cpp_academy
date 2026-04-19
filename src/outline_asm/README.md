# outline_asm — Assembly em Arquivo Separado (Out-of-Line)

## O que é este projeto?

Demonstra como usar **assembly em arquivos `.s` separados** (out-of-line assembly), em vez de assembly inline no meio do código C++. Esta abordagem é preferível para rotinas assembly mais longas, complexas ou que precisam ser compiladas com ferramentas externas.

---

## Inline vs Outline Assembly

| Aspecto | Inline (`asm(...)`) | Outline (arquivo `.s`) |
|---|---|---|
| Localização | Dentro do `.cpp` | Arquivo `.s` separado |
| Complexidade | Bom para 1–5 instruções | Melhor para rotinas longas |
| Tooling | GCC extended asm | NASM, AS, ou GAS completo |
| Otimizador | Pode interferir | Código assembly exato |
| Portabilidade | GCC/Clang apenas | Qualquer assembler |
| Manutenção | Difícil de ler | Separado, comentável |

---

## Estrutura de arquivos

```
outline_asm/
├── main.cpp       ← código C++ que chama as funções assembly
├── asm_funcs.hpp  ← declarações das funções (interface C++)
├── asm_funcs.s    ← implementação em assembly (arquivo separado)
└── meson.build
```

---

## Interface C++ — `asm_funcs.hpp`

```cpp
// Declara funções implementadas em assembly para o compilador C++
// "extern C" evita name mangling do C++ (assembly usa nomes simples)
extern "C" {
    // Soma dois inteiros de 32 bits
    int asm_add(int a, int b);

    // Multiplica dois inteiros e retorna resultado de 64 bits
    long long asm_mul64(int a, int b);

    // Copia N bytes de src para dst
    void asm_memcpy(void* dst, const void* src, size_t n);

    // Retorna o maior de dois valores
    int asm_max(int a, int b);
}
```

---

## Implementação assembly — `asm_funcs.s`

```asm
# asm_funcs.s — Funções em AT&T syntax (GAS)
# Compilado como: as -o asm_funcs.o asm_funcs.s

.section .text

# ─── asm_add: retorna a + b ───────────────────────────────────────
# ABI x86-64 System V:
#   Argumentos: edi=a, esi=b
#   Retorno: eax
.globl asm_add
asm_add:
    movl  %edi, %eax    # eax = a
    addl  %esi, %eax    # eax = a + b
    ret

# ─── asm_max: retorna max(a, b) ──────────────────────────────────
.globl asm_max
asm_max:
    movl  %edi, %eax    # eax = a (candidato a retorno)
    cmpl  %esi, %edi    # compara a com b
    cmovl %esi, %eax    # se a < b: eax = b (conditional move, sem branch!)
    ret

# ─── asm_memcpy: copia n bytes ────────────────────────────────────
# rdi = dst, rsi = src, rdx = n
.globl asm_memcpy
asm_memcpy:
    test  %rdx, %rdx
    jz    .done
    movq  %rdx, %rcx    # rcx = n (contador para rep)
    rep movsb            # copia [rsi] → [rdi], rcx vezes, incrementa ambos
.done:
    ret
```

---

## ABI x86-64 System V (Linux)

Para que C++ e assembly se comuniquem, devem concordar sobre como passar argumentos e retornar valores:

```
Argumentos inteiros/ponteiros (ordem):
  1º: rdi  2º: rsi  3º: rdx  4º: rcx  5º: r8  6º: r9

Argumentos float/double (ordem):
  xmm0, xmm1, xmm2, ... xmm7

Retorno:
  Inteiro/ponteiro: rax (e rdx para 128-bit)
  Float/double: xmm0

Registradores preservados pelo callee (callee-saved):
  rbx, rbp, r12, r13, r14, r15
  (se o asm usar esses, deve salvar e restaurar)

Registradores que o caller pode perder (caller-saved):
  rax, rcx, rdx, rsi, rdi, r8–r11, xmm0–xmm15
```

---

## Usando no `main.cpp`

```cpp
#include "asm_funcs.hpp"
#include <fmt/core.h>

int main() {
    int soma = asm_add(10, 32);
    fmt::print("asm_add(10, 32) = {}\n", soma);  // 42

    int maior = asm_max(17, 42);
    fmt::print("asm_max(17, 42) = {}\n", maior); // 42

    char src[] = "hello, assembly!";
    char dst[64] = {};
    asm_memcpy(dst, src, sizeof(src));
    fmt::print("asm_memcpy: {}\n", dst); // "hello, assembly!"
}
```

---

## `rep movsb` — cópia otimizada de memória

`rep movsb` é uma instrução de string do x86 que copia `rcx` bytes de `[rsi]` para `[rdi]`, incrementando ambos os ponteiros após cada byte:

```
REP MOVSB:
  while (rcx > 0) {
      [rdi] = [rsi];
      rdi++;
      rsi++;
      rcx--;
  }
```

Em CPUs modernas, o hardware detecta `rep movsb` e usa movimentos de 16/32/64 bytes internamente (ERMSB — Enhanced REP MOVSB/STOSB), tornando-o equivalente ou superior a loops manuais com SSE/AVX para tamanhos arbitrários.

---

## `cmov` — conditional move sem branch

```asm
# asm_max sem branch (branch-free):
asm_max:
    movl  %edi, %eax   # eax = a
    cmpl  %esi, %edi   # compara a com b (afeta flags)
    cmovl %esi, %eax   # SE a < b ENTÃO eax = b (conditional move)
    ret
# Se executado com branch, o processador poderia predizer errado 50% das vezes.
# cmov é sempre executado, sem branch — melhor para dados imprevisíveis.
```

---

## Build system — integrando `.s` no Meson

```python
# meson.build
asm_src = files('asm_funcs.s')
cpp_src = files('main.cpp')

executable('outline_asm',
    [cpp_src, asm_src],  # .s é compilado pelo assembler automaticamente
    # ...)
```

O Meson detecta arquivos `.s` e usa o assembler correto (GAS no Linux).

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja outline_asm
./bin/outline_asm
```

---

## Dependências

- `fmt` — formatação de saída
- GAS (GNU Assembler) — incluso no pacote `binutils`
