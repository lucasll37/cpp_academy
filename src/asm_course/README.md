# asm_course — Curso de Assembly x86-64, do zero

## O que é este projeto?

Quinze módulos independentes que ensinam **assembly x86-64** partindo do absoluto zero — sem
supor que você já saiba o que é um registrador — até escrever `memcpy`, `strlen` e um `sort`
à mão e medir se valeu a pena.

Cada módulo é um executável próprio. Rode na ordem:

```bash
make build
./dist/bin/asm_01_registers
./dist/bin/asm_02_memory
# ... até asm_15_capstone
```

## Por que um programador de C++ estudaria assembly?

Não é para escrever assembly. Em 2026, quase ninguém escreve — e quando escreve, o compilador
quase sempre faz melhor. A razão é outra, e é prática:

| O que você passa a conseguir | Sem assembly |
|---|---|
| **Ler o que o compilador gerou** (`-S`, `objdump`, godbolt) | você confia ou reza |
| Entender por que `-O2` mudou o resultado | "otimização quebrou meu código" |
| Depurar um crash sem símbolos | a pilha é um beco sem saída |
| Entender ABI, alinhamento e convenção de chamada | bugs que "não fazem sentido" |
| Saber o que SIMD faz de verdade | intrínsecos viram magia |

Este subprojeto tem **prova disso**. Ao habilitar todos os alvos do repositório, dois binários
daqui deram **segfault** — e os dois eram bugs de **ABI**, do tipo que só se enxerga sabendo
assembly. Estão documentados nos módulos 7 e 10, e são o material mais valioso do curso.

---

## Antes do módulo 1: os cinco conceitos que você precisa

Se algum destes for novo, leia esta seção inteira antes de rodar qualquer coisa.

### 1. O que é um registrador

O processador não sabe somar dois números que estão na memória RAM. Ele só opera sobre
**registradores**: um punhado de gavetas de 64 bits *dentro* do próprio processador, com nome
próprio. Todo cálculo é um vaivém:

```
memória ──(load)──▶ registrador ──(operação)──▶ registrador ──(store)──▶ memória
```

O x86-64 tem 16 registradores de uso geral: `RAX RBX RCX RDX RSI RDI RBP RSP R8..R15`.
Comparado à RAM (bilhões de bytes), são **128 bytes no total**. Essa escassez explica
metade do assembly que você vai ler.

### 2. Os aliases: RAX, EAX, AX, AL

O mesmo registrador tem quatro nomes, para quatro tamanhos:

```
┌───────────────────────── RAX (64 bits) ─────────────────────────┐
                                 ┌──────── EAX (32 bits) ─────────┐
                                                 ┌── AX (16) ─────┐
                                                        ┌─ AL(8) ─┐
```

> ⚠ **A regra que morde:** escrever em `EAX` **zera** os 32 bits de cima de `RAX`.
> Escrever em `AX` ou `AL` **não** zera nada. É a origem de bugs em que "o valor tem lixo
> na parte alta". Módulo 1 mede isso.

### 3. O que é a pilha

Uma região de memória que cresce **para baixo** (endereços decrescentes). `RSP` aponta para o
topo. Duas instruções a manipulam:

```
pushq %rax     # RSP -= 8;  memória[RSP] = RAX
popq  %rax     # RAX = memória[RSP];  RSP += 8
```

É onde vivem as variáveis locais e os endereços de retorno. Módulos 6 e 7.

### 4. O que são as flags

Um registrador especial, `RFLAGS`, cujos bits o processador atualiza **como efeito colateral**
de operações aritméticas:

| flag | nome | liga quando |
|---|---|---|
| `ZF` | Zero | o resultado deu zero |
| `SF` | Sign | o resultado é negativo |
| `CF` | Carry | houve "vai um" além do bit mais alto (sem sinal) |
| `OF` | Overflow | o resultado estourou a faixa **com sinal** |

`CMP a, b` é uma subtração que **descarta o resultado e guarda só as flags**. É assim que todo
`if` funciona em assembly. Módulos 3 e 5.

### 5. As duas sintaxes

Você vai encontrar as duas, e elas invertem a ordem dos operandos:

```asm
;  Intel (documentação da Intel, MSVC, NASM)
mov  rax, rbx      ; destino ← origem

#  AT&T (GNU as, GCC, este curso)
movq %rbx, %rax    # origem → destino
```

Na AT&T os registradores levam `%`, os imediatos levam `$`, e o sufixo diz o tamanho
(`b`=8, `w`=16, `l`=32, `q`=64). Para ver a Intel: `objdump -d -M intel`.

---

## Os quinze módulos

| # | Módulo | O que ensina |
|---|---|---|
| 01 | `registers` | os 16 registradores, os aliases, a zeragem implícita de EAX |
| 02 | `memory` | modos de endereçamento, `LEA` (que **não** acessa memória) |
| 03 | `arithmetic` | `ADD/SUB/MUL/DIV`, e as flags como efeito colateral |
| 04 | `bitwise` | `AND/OR/XOR/NOT`, shifts, rotações, o XOR swap |
| 05 | `control_flow` | `CMP` + `Jcc`, `CMOV` e `SETcc` — o `if` sem desvio |
| 06 | `stack_frames` | `PUSH/POP`, `CALL/RET`, prólogo e epílogo |
| 07 | `calling_conv` | **System V AMD64 ABI** completo, incluindo variádicos |
| 08 | `simd_sse` | registradores XMM, 4 floats de uma vez |
| 09 | `simd_avx` | YMM (256 bits), 8 floats, e o FMA |
| 10 | `string_ops` | `REP MOVS/STOS/SCAS/CMPS` e o direction flag |
| 11 | `syscalls` | `SYSCALL` direto, sem libc nenhuma |
| 12 | `fp_x87` | a FPU x87 — legado, mas ainda no silício |
| 13 | `optimization` | branchless, pipeline, prefetch, loop unrolling |
| 14 | `disassembly` | como **ler** o que o GCC gerou |
| 15 | `capstone` | projeto final: `memcpy`, `strlen`, `sort` em assembly |

Os módulos 6 a 15 têm um arquivo `.s` companheiro — assembly **puro**, montado pelo `as` e
linkado ao C++. É a diferença entre `inline_asm` (assembly dentro do C++) e `outline_asm`
(arquivos separados), os dois subprojetos vizinhos.

---

## ⚠ Os dois bugs de ABI que este curso encontrou

Estes dois estavam **latentes** no repositório, escondidos por um `subdir()` comentado. Ao
habilitar todos os alvos, os dois binários passaram a dar **SIGSEGV**. Eles valem mais que
qualquer explicação teórica de por que a ABI existe.

### Bug 1 — o alinhamento de pilha (módulo 7)

A regra do System V AMD64 ABI:

> No momento em que a instrução `CALL` é **executada**, `RSP` precisa ser múltiplo de **16**.

Consequência: ao **entrar** numa função, `RSP ≡ 8 (mod 16)` — porque o `call` que trouxe você
até ali empurrou 8 bytes de endereço de retorno.

O código estava assim, com um comentário que afirmava o contrário do que a conta diz:

```asm
asm_apply:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $8, %rsp        # ⚠ o comentário dizia que isto ALINHAVA
    call    *%rax
```

Refazendo a conta:

```
entrada ............ RSP ≡ 8  (mod 16)
pushq %rbp ......... RSP ≡ 0   ← JÁ estava alinhado
subq $8, %rsp ...... RSP ≡ 8   ← e aqui QUEBRAMOS
call *%rax ......... o destino recebe RSP ≡ 0   ✗
```

O destino esperava `RSP ≡ 8` na entrada. Com `RSP ≡ 0` ele calcula todos os seus offsets
deslocados por 8 — e qualquer `movaps` (load SSE **alinhado**, que o compilador emite à
vontade em `-O2`) recebe um endereço que não é múltiplo de 16 e dispara `SIGSEGV`.

**O sintoma era desconcertante.** A pilha mostrava o crash quatro quadros acima, dentro do fmt:

```
#0  fmt::v10::detail::vformat_to<char>
#4  cpp_callback  (07_calling_conv.cpp:52)
#5  asm_apply     (07_calling_asm.s:124)   ← o culpado
```

...e o fmt estava perfeito. **É a marca registrada do bug de alinhamento: ele estoura em quem
usa SSE, nunca em quem quebrou o alinhamento.** A correção foi remover o `subq`.

### Bug 2 — a promessa quebrada ao compilador (módulo 10)

```cpp
asm volatile ("std\n\t  rep movsb\n\t  cld"
    :                                              // ⚠ nenhuma saída
    : "D"(&rev[7]), "S"(&arr[7]), "c"((size_t)8)   // ⚠ só entradas
    : "memory");
```

`rep movsb` **modifica** os três registradores: incrementa/decrementa `RDI` e `RSI` a cada
byte, e zera `RCX`. Declarar um registrador como **entrada** é uma **promessa** ao compilador
de que o bloco não o altera — e o GCC confia: mantém outros valores vivos ali e os reutiliza
depois.

O crash aparecia na linha **seguinte** ao `asm`, num laço que lia um ponteiro corrompido.

A correção é declará-los **read-write** com `+`:

```cpp
void* d = &rev[7];  const void* o = &arr[7];  std::size_t n = 8;
asm volatile ("std\n\t  rep movsb\n\t  cld"
    : "+D"(d), "+S"(o), "+c"(n)     // ← o bloco os ALTERA
    :
    : "memory");
```

> ⚠ Não se pode listar `RDI`/`RSI`/`RCX` no *clobber*: o GCC recusa clobbers que colidam com
> operandos. É exatamente por isso que o `+` existe.

---

## Como validar

```bash
make build
for i in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15; do
    ./dist/bin/asm_$i* >/dev/null || echo "FALHOU: módulo $i"
done
```

Estado atual: **15/15 módulos compilam sem warnings e executam com código 0.**

Para ver o assembly que o *compilador* gera a partir do seu C++:

```bash
g++ -O2 -S -masm=intel arquivo.cpp -o -      # assembly, sintaxe Intel
objdump -d -M intel ./binario | less         # do binário já montado
gdb -batch -ex 'disassemble main' ./binario
```

E o site que vale mais que qualquer um deles: **godbolt.org** (Compiler Explorer), que mostra
fonte e assembly lado a lado, coloridos por correspondência.

## Referências

- [`src/inline_asm/`](../inline_asm/) — assembly **dentro** do C++: constraints e clobbers
- [`src/outline_asm/`](../outline_asm/) — arquivos `.s` separados e a ABI
- [`src/bits/`](../bits/) — as mesmas operações vistas do lado do C++
- [`src/layout/`](../layout/) — por que o hardware exige alinhamento
- [`src/performance/`](../performance/) — cache, predição de desvio e SIMD, medidos
- *Intel 64 and IA-32 Architectures Software Developer's Manual* — a fonte canônica
- Agner Fog, *Optimizing subroutines in assembly language* — as tabelas de latência
- *System V AMD64 ABI* — o documento que os dois bugs acima violavam
