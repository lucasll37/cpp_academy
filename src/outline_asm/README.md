# outline_asm — Assembly em arquivo separado, linkado ao C++

## O que é este projeto?

Dez funções escritas **inteiramente em assembly x86-64**, num arquivo `.s` à parte, que o C++ chama
como se fossem funções C++ normais. O assunto é a **fronteira**: o que precisa ser verdade para que um
arquivo de assembly e um `.cpp`, compilados por ferramentas diferentes, se encontrem e funcionem juntos.

```bash
make configure          # só na primeira vez (Conan + Meson)
make build
./dist/bin/outline_asm
```

O programa roda as dez funções e compara cada resultado com uma referência C++. Espera-se **zero**
linhas `✗ FALHOU` na saída.

## Antes de começar: os conceitos que você precisa

Nada aqui presume mais que C++ básico — sintaxe, funções, `int`.

### 1. O que acontece quando você "compila", e o que é um símbolo

O que chamamos de compilar são dois programas distintos rodando em sequência:

```
main.cpp    ──[compilador g++]──►  main.o       ┐
                                                ├──[linker ld]──►  executável
asm_funcs.s ──[assembler as ]──►  asm_funcs.o  ┘
```

O **compilador** traduz C++ e produz um **arquivo objeto** (`.o`); ele nunca vê os outros arquivos do
projeto. O **assembler** faz o mesmo com um `.s` — que já *é* a lista de instruções em texto, então a
tradução é literal, sem otimização. O **linker** junta os `.o` e resolve as referências entre eles.
`main.o` e `asm_funcs.o` vêm de ferramentas que nunca se falaram: só se encontram no linker, e a única
coisa que os liga é um **nome**.

Um **símbolo** é justamente isso: um nome com um endereço associado dentro de um arquivo objeto — cada
função e cada variável global vira um. Todo objeto carrega duas listas, os símbolos que **define** ("a
função `asm_soma_int` começa no byte 0 do meu código") e os que **usa sem definir** ("eu chamo
`asm_soma_int`, alguém me diga onde ela está"). A ferramenta `nm` mostra as duas: **`T`** = definido e
global, na seção `.text` (código); **`U`** = indefinido.

O trabalho do linker é casar cada `U` com um `T` de mesmo nome. Sobrou `U` sem par →
`undefined reference`; dois `T` iguais → `multiple definition`. A linkagem inteira é um casamento de
strings — e é por isso que o nome importa tanto na próxima seção.

### 2. Name mangling e o `extern "C"`

C++ tem sobrecarga: `f(int)` e `f(double)` são funções diferentes de mesmo nome. Como o símbolo precisa
ser único, o compilador **codifica os tipos dos parâmetros dentro do nome**. Isso é *name mangling*:

```console
$ cat mang.cpp
int soma_cpp(int a, int b) { return a + b; }
extern "C" int soma_c(int a, int b) { return a + b; }
$ g++ -c mang.cpp -o mang.o && nm --defined-only mang.o
0000000000000000 T _Z8soma_cppii    ← "_Z", 8 letras, "soma_cpp", (i)nt, (i)nt
0000000000000018 T soma_c           ← o nome literal, sem enfeite
```

O assembler **não faz mangling**: o símbolo exportado por `asm_funcs.s` é a string literal
`asm_soma_int`. Sem `extern "C"`, o `.cpp` procuraria por `_Z12asm_soma_intii` e o linker não acharia
par. Note que `extern "C"` **não muda como a função é chamada** nem a passagem de argumentos — muda uma
coisa só, o nome do símbolo. Por isso `asm_funcs.hpp` envolve as dez declarações num único bloco
`extern "C" { ... }`.

### 3. O ciclo completo, no menor exemplo possível

Este exercício de 30 segundos contém o subprojeto inteiro em miniatura.

```gas
# resposta.s
    .section .text
    .globl resposta                 # torna o nome visível ao linker
    .type  resposta, @function
resposta:                           # aqui começa o símbolo
    movl    $42, %eax               # o retorno vai em EAX
    ret                             # volta para quem chamou
    .size  resposta, .-resposta
```

```console
$ gcc -c resposta.s -o resposta.o && nm resposta.o
0000000000000000 T resposta          ← o símbolo existe e é global
$ cat rmain.cpp
#include <cstdio>
extern "C" int resposta();
int main() { printf("resposta() = %d\n", resposta()); }
$ g++ rmain.cpp resposta.o -o rmain && ./rmain
resposta() = 42
```

Nada além disso acontece em `asm_funcs.s` — só que dez vezes, com mais instruções.

### 4. As cinco diretivas GAS

Linhas iniciadas por ponto **não são instruções**: são ordens ao assembler e não geram byte algum de
código.

| Diretiva | O que faz |
|---|---|
| `.section .text` | daqui em diante, tudo vai na seção de **código executável** |
| `.globl nome` | **exporta** o símbolo; sem isto ele fica local (`t` minúsculo no `nm`) e o linker não o enxerga |
| `.type nome, @function` | marca o símbolo como função (usado por depurador e por linkagem dinâmica) |
| `.size nome, .-nome` | tamanho da função: `.` é "o endereço atual", logo `.-nome` = bytes desde o rótulo |
| `.section .note.GNU-stack,"",@progbits` | declara que este objeto **não** precisa de pilha executável |

⚠ **A última é obrigatória em todo `.s` escrito à mão.** Sem ela o linker não sabe se o objeto exige
pilha executável, assume o pior caso e emite `missing .note.GNU-stack section implies executable stack`.
O efeito é medível: o executável inteiro fica com a pilha marcada `RWE` (leitura, escrita e **execução**),
anulando uma proteção contra exploits.

```console
$ readelf -lW rmain | grep GNU_STACK                 →  GNU_STACK ... RWE 0x10   # sem a nota
$ readelf -lW dist/bin/outline_asm | grep GNU_STACK   →  GNU_STACK ... RW  0x10   # com a nota
```

### 5. Sintaxe AT&T e os sufixos de tamanho

O GAS usa sintaxe AT&T, que inverte a ordem dos operandos frente à sintaxe Intel dos manuais:

```
AT&T:   movl  %edi, %eax        # origem PRIMEIRO, destino DEPOIS
Intel:  mov   eax, edi          # destino primeiro
```

Registradores levam `%` e constantes levam `$` (`$42` é o número; `42` seria um endereço), e a instrução
leva um **sufixo de tamanho**: `b` = 1 byte, `w` = 2, `l` = 4 (*long*), `q` = 8 (*quad*). Os nomes de
registrador mudam conforme a largura, mas são **partes do mesmo registrador físico**:

```
        63              31            15    7   0
%rax    ├───────────────┼─────────────┼─────┼───┤
%eax                    ├─────────────┼─────┼───┤   32 bits baixos
%ax                                   ├─────┼───┤   16 bits baixos
%al                                         ├───┤    8 bits baixos
```

⚠ Regra do x86-64 que confunde todo mundo: **escrever num registrador de 32 bits zera os 32 bits
altos**; escrever num de 8 ou 16 bits **não zera nada**. Por isso `xorl %eax,%eax` limpa `%rax` inteiro
e `xorb %al,%al` deixaria lixo do chamador nos 56 bits de cima (veja o demo 7).

### 6. O que é uma ABI, e quem preserva cada registrador

**ABI** (*Application Binary Interface*) é um **acordo** — escrito, publicado, versionado — sobre o
comportamento do código de máquina na fronteira entre funções compiladas separadamente: em que
registrador vai cada argumento, onde fica o retorno, quem preserva o quê, como a pilha está alinhada.
Não é característica do processador; é convenção. No Linux x86-64 chama-se **System V AMD64 ABI**, e
GCC, Clang e glibc a seguem. Você **não pode violá-la unilateralmente**: o compilador do outro lado
continuará confiando nela, e o programa quebra num ponto sem relação visível com a causa.

```
Argumentos inteiros/ponteiros, na ordem:
   1º %rdi   2º %rsi   3º %rdx   4º %rcx   5º %r8   6º %r9     (7º em diante: pela pilha)
Argumentos float/double:   %xmm0 … %xmm7
Retorno:   inteiro/ponteiro → %rax  (128 bits: %rax + %rdx)    float/double → %xmm0
```

São 16 registradores de uso geral e duas funções — a que chama e a chamada — querendo usar todos. A ABI
divide a responsabilidade:

| Categoria | Registradores | Quem preserva |
|---|---|---|
| **caller-saved** (voláteis) | `%rax %rcx %rdx %rsi %rdi %r8–%r11`, `%xmm0–%xmm15` | **quem chama**: salva antes do `call` se ainda precisar do valor. A função chamada suja à vontade. |
| **callee-saved** (preservados) | `%rbx %rbp %r12 %r13 %r14 %r15` | **quem é chamado**: se usar, devolve com o valor original — `pushq` na entrada, `popq` antes do `ret`. |

Invertendo: *caller-saved* é a lista do que a sua função assembly pode sujar de graça; *callee-saved* é
a lista do que exige `push`/`pop`.

⚠ Corromper um callee-saved é o bug mais cruel do assunto: nada explode na sua função — o chamador é
que, muitas linhas depois, lê valor errado de uma variável que guardava em `%rbx`. Neste projeto só
`asm_eh_primo` (demo 10) usa um.

### 7. `call`, `ret` e o alinhamento de 16 bytes

`call f` faz duas coisas: **empurra na pilha o endereço da instrução seguinte** (8 bytes) e salta para
`f`. `ret` faz o inverso — desempilha esse endereço e salta para ele. `ret` não é palavra mágica: é
"pop + jump" no endereço de retorno que o `call` deixou.

```
antes do call:  ... ◄─rsp     depois do call:  [end.retorno] ◄─rsp     depois do ret:  ... ◄─rsp
```

⚠ A ABI exige `%rsp` **múltiplo de 16 no momento em que o `call` executa**. Como o `call` empurra 8
bytes, ao entrar na sua função `%rsp` vale `múltiplo de 16 − 8`. Se a sua função não chama ninguém, isso
não incomoda; se ela **vai chamar** outra (da libc, por exemplo), empurre um número **ímpar** de qwords
antes (ou `subq $8, %rsp`). Violar isso dá `SIGSEGV` dentro da libc, numa instrução `movaps` que exige
endereço alinhado a 16, com stack trace apontando para a função errada —
[`../asm_course/`](../asm_course/) documenta esse bug com o trace real.

## Por que estudar isto?

Assembly inline (`asm(...)` no meio do C++, assunto de [`../inline_asm/`](../inline_asm/)) serve para
uma ou duas instruções e cobra caro: é preciso descrever ao compilador, na sintaxe de *constraints* do
GCC, o que entra, o que sai e o que é destruído — e errar essa descrição gera bugs que só aparecem em
`-O2`.

| | Inline `asm(...)` | Arquivo `.s` (out-of-line) |
|---|---|---|
| Tamanho útil | 1–5 instruções | rotinas inteiras, com laços e rótulos |
| Quem escolhe os registradores | o compilador (você declara constraints) | **você**; a ABI é o único contrato |
| Onde o erro aparece | ao mudar o nível de otimização | ao violar a ABI |
| Ferramentas | só GCC/Clang, sintaxe própria | qualquer assembler; `nm`, `objdump`, `gdb` direto |
| Legibilidade | ruim (escapes, `%%`, constraints) | assembly puro, igual ao dos manuais |

Para quem está aprendendo, o motivo decisivo: **é a única forma de escrever uma função inteira em
assembly e de ver a ABI funcionando de verdade.** No inline, o compilador remenda as bordas por você;
aqui não há remendo — se `%rdi` não for o primeiro argumento, nada funciona.

## Estrutura de arquivos

```
src/outline_asm/
├── main.cpp        ← chama as 10 funções e compara com referência C++
├── asm_funcs.hpp   ← as 10 declarações dentro de um extern "C"
├── asm_funcs.s     ← a implementação: assembly x86-64 AT&T/GAS
├── meson.build     ← o .s entra na mesma lista de fontes que o .cpp
└── README.md
```

O `meson.build` não tem truque nenhum — o Meson vê a extensão `.s` e chama o assembler:

```python
executable('outline_asm', ['main.cpp', 'asm_funcs.s'],
    dependencies: [fmt_dep], include_directories: include_directories('.'), install: true)
```

As dez demos do `main.cpp` seguem o mesmo padrão: cada resultado do assembly passa por
`ok(cond, texto)`, que imprime `✓` ou `✗ FALHOU` comparando com um oráculo C++ independente (`strlen`
da libc, uma lambda de fatorial, uma de primalidade). Isso não é decoração. Assembly escrito à mão não
tem verificação de tipos, nem checagem de limites, nem quem avise que você trocou `-1` por `-2`. A única
defesa é uma referência ao lado — e foi exatamente esse `ok(...)` que deixou visível na tela o bug de
`asm_strlen` descrito no demo 7.

## Os demos, um a um

Na ordem em que rodam. A dificuldade cresce: 1–2 leem argumentos, 3–6 fazem contas, 7–9 usam laços e
instruções de string, 10 é o único que mexe num callee-saved.

### Demo 1 — `asm_soma_int`: ler argumentos e devolver o resultado

```gas
    .globl asm_soma_int
    .type  asm_soma_int, @function
asm_soma_int:
    movl    %edi, %eax      # eax = a   (1º argumento chega em %rdi/%edi)
    addl    %esi, %eax      # eax += b  (2º argumento em %rsi/%esi)
    ret                     # retorno em %eax, por definição da ABI
    .size asm_soma_int, .-asm_soma_int
```

Três instruções e o subprojeto já está exercitado: `.globl` cria o símbolo, a ABI diz onde estão `a` e
`b`, `%eax` carrega o retorno. Como `int` tem 32 bits, usam-se `%edi`/`%eax` (sufixo `l`), não
`%rdi`/`%rax`.

### Demo 2 — `asm_soma_int64`: as mesmas três instruções em 64 bits

```gas
asm_soma_int64:
    movq    %rdi, %rax      # q = quad word = 8 bytes
    addq    %rsi, %rax
    ret
```

A única diferença é o sufixo `l` → `q` e o nome do registrador. Na tela aparece
`INT32_MAX + 1 = 2147483648`, que em `int` teria transbordado.

### Demo 3 — `asm_valor_absoluto`: o primeiro truque sem desvio

`|n|` sem nenhum `if`, usando complemento de dois:

```gas
asm_valor_absoluto:
    movl    %edi, %eax      # eax = n
    movl    %edi, %edx      # edx = n
    sarl    $31,  %edx      # shift ARITMÉTICO: propaga o bit de sinal
                            #   n >= 0 → edx = 0x00000000   |   n < 0 → edx = 0xFFFFFFFF (= -1)
    xorl    %edx, %eax      # n>=0: n XOR 0 = n     |  n<0: n XOR 0xFF..FF = ~n
    subl    %edx, %eax      # n>=0: n - 0 = n       |  n<0: ~n - (-1) = ~n+1 = -n  ✓
    ret
```

`sar` (*shift arithmetic right*) preenche pela esquerda com o **bit de sinal**, ao contrário de `shr`,
que preenche com zero; deslocar 31 posições transforma o sinal numa **máscara** de tudo-zero ou
tudo-um. E `~n + 1` é a definição de negação em complemento de dois.

### Demo 4 — `asm_fatorial`: o primeiro laço

```gas
asm_fatorial:
    movl    %edi,   %ecx    # ecx = n (contador)
    movq    $1,     %rax    # rax = 1 (acumulador)
    testl   %ecx,   %ecx    # n == 0?
    jle     .fat_fim        # n <= 0 → retorna 1
.fat_loop:
    imulq   %rcx,   %rax    # rax *= rcx
    decl    %ecx            # ecx--
    jnz     .fat_loop       # repete enquanto ecx != 0
.fat_fim:
    ret
```

Três novidades: **rótulos locais** (o ponto inicial em `.fat_loop` impede que vire símbolo exportado),
**flags** e **saltos condicionais**. `testl %ecx,%ecx` faz um AND descartando o resultado, só para
atualizar as flags que `jle`/`jnz` leem. `%rcx` é caller-saved, dispensa `push`.

⚠ O contrato do header é `n <= 20`: `21!` não cabe em 64 bits, e o assembly não verifica — não há
exceção, só silêncio e um número errado.

### Demo 5 — `asm_maximo`: movimento condicional (CMOV)

```gas
asm_maximo:
    movl    %edi, %eax      # eax = a  (chute inicial)
    cmpl    %esi, %edi      # compara a com b — só mexe nas flags
    cmovll  %esi, %eax      # se a < b (l = less), eax = b
    ret
```

Um desvio (`jl`) força a CPU a *adivinhar* o caminho antes de conhecer o resultado da comparação. CPUs
modernas executam dezenas de instruções adiantadas numa esteira (o *pipeline*); quando o palpite erra —
*branch misprediction* — todo o trabalho especulativo é descartado e a esteira, reenchida: 15–20 ciclos
típicos. Para dados imprevisíveis (50% de acerto), isso é caro. `cmov` não é desvio: a instrução
**sempre executa** e só a escrita no destino depende das flags, então não há o que prever errado. Note o
duplo `l` em `cmovll`: o primeiro é a condição (*less*), o segundo é o sufixo de tamanho (*long*).

⚠ `cmov` não é sempre melhor: cria dependência de dados e avalia os dois lados. Quando o desvio é bem
previsível — um laço, uma validação que quase nunca falha — o `jl` ganha.

### Demo 6 — `asm_popcount`: quando a instrução já existe

```gas
asm_popcount:
    popcntl %edi, %eax      # conta os bits 1 de edi
    ret
```

Contar bits 1 em software leva um laço ou um algoritmo de máscaras; desde o SSE4.2 (Intel Nehalem, 2008;
AMD via ABM) existe uma instrução que faz isso direto.

⚠ `popcnt` **não existe** em CPUs anteriores: num x86-64 antigo o programa morre com `SIGILL` (instrução
ilegal). O equivalente portátil é `std::popcount` (C++20), que o compilador reduz a esta mesma instrução
quando pode.

### Demo 7 — `asm_strlen`: instruções de string e a conta do `repne`

```gas
asm_strlen:
    xorl    %eax,   %eax    # al = 0 (o byte procurado: '\0')
    movq    $-1,    %rcx    # contador "infinito"
    repne   scasb           # rcx--, compara AL com [rdi], rdi++ — para se igual
    notq    %rcx
    leaq    -1(%rcx), %rax
    ret
```

`scasb` (*scan string byte*) compara `%al` com o byte em `[%rdi]` e incrementa `%rdi`. O prefixo `repne`
(*repeat while not equal*) transforma isso num laço de hardware: decrementa `%rcx`, executa o `scasb` e
para quando a comparação der igual **ou** `%rcx` zerar. Daí `%rcx` começar em `-1`: é o maior contador
possível, para que só o `\0` interrompa. A conta, para uma string de comprimento `L`:

```
%rcx inicial            = -1
bytes varridos          = L + 1                (os L caracteres + o '\0')
%rcx final              = -1 - (L+1) = -(L+2)
notq %rcx  → ~(-(L+2))  = (L+2) - 1  = L + 1   (pois ~x == -x - 1)
leaq -1(%rcx), %rax     = L                    ✓ desconta o '\0'
```

⚠ **O bug que estava aqui.** A linha era `leaq -2(%rcx), %rax`, com o comentário "-1: o '\0' foi
contado, -1: ajuste do REPNE" — uma justificativa plausível para uma subtração a mais. O `notq` já faz o
ajuste do `repne`, então o resultado saía sempre `L-1`:

```
antes:  strlen("") = -1   strlen("a") = 0   strlen("hello") = 4   ✗ FALHOU
depois: strlen("") =  0   strlen("a") = 1   strlen("hello") = 5   ✓
```

Duas correções acompanham: havia um `movq %rdi, %rcx` **morto** (sobrescrito três linhas depois) cujo
comentário descrevia uma técnica — guardar o ponteiro e subtrair no fim — que o código não usava; e
`xorb %al,%al` virou `xorl %eax,%eax`, porque zerar só o `AL` deixa lixo do chamador nos 56 bits altos
de `%rax`, justamente o registrador de retorno.

⚠ A direção do `rep` depende da flag `DF`. A ABI garante `DF` zerada (incremento) na entrada de toda
função e exige devolvê-la zerada; este código depende disso e não viola.

### Demo 8 — `asm_memcpy_64`: cópia em blocos com `rep movsq`

```gas
asm_memcpy_64:                  # void asm_memcpy_64(void* dst, const void* src, int64_t n)
    movq    %rdx, %rcx          # rcx = n  (rep sempre conta em rcx)
    rep     movsq               # copia rcx qwords: [rsi]→[rdi], rsi+=8, rdi+=8
    ret
```

⚠ **`n` é o número de blocos de 8 bytes, não de bytes.** `movsq` move uma *quad word* por iteração; para
copiar 4 qwords (32 bytes) o `main.cpp` passa `4`. Passar `sizeof(src)` aqui copiaria 8× mais do que
existe.

⚠ Não há guarda para `n == 0` — e não precisa: `rep` com `%rcx == 0` executa zero iterações. Também não
há tratamento de sobreposição entre `dst` e `src` (isso seria `memmove`) nem de tamanhos que não sejam
múltiplos de 8.

Existe a variante byte a byte, `rep movsb`, que aceita qualquer tamanho. Ela é a base do ERMSB
(*Enhanced REP MOVSB/STOSB*), recurso presente desde a microarquitetura Ivy Bridge em que o hardware
**reconhece** o padrão e internamente move 16, 32 ou 64 bytes por vez, o que a torna competitiva com
laços SSE/AVX à mão. O módulo 10 de [`../asm_course/`](../asm_course/) trata instruções de string a fundo.

### Demo 9 — `asm_soma_array`: laço com ponteiro e extensão de sinal

```gas
asm_soma_array:
    xorq    %rax, %rax      # acumulador = 0
    testl   %esi, %esi      # n == 0?
    jle     .arr_fim        # n <= 0 → devolve 0
    movl    %esi, %ecx      # ecx = n
.arr_loop:
    movslq  (%rdi), %rdx    # rdx = *arr, estendendo o sinal de 32 → 64 bits
    addq    %rdx,   %rax
    addq    $4,     %rdi    # arr++ → 4 bytes, o tamanho de um int
    decl    %ecx
    jnz     .arr_loop
.arr_fim:
    ret
```

`(%rdi)` com parênteses significa **"o conteúdo do endereço em `%rdi`"**; sem parênteses seria o próprio
ponteiro. E `arr++` em C++ anda `sizeof(int)` bytes — em assembly essa multiplicação é responsabilidade
sua, daí o `addq $4`. `movslq` = *move with sign extension, long → quad*: somar `int32` num acumulador
`int64` só está correto se o sinal for propagado, pois um `-3` de 32 bits (`0xFFFFFFFD`) copiado cru
para 64 bits viraria 4294967293. O demo testa isso com `{-3,-2,-1,0,1,2,3}`, que deve somar 0.

### Demo 10 — `asm_eh_primo`: o único callee-saved do projeto

```gas
asm_eh_primo:
    pushq   %rbx                # ⚠ %rbx é callee-saved: salvar é obrigatório
    ...                         # (n < 2 → não; n == 2 → sim; n par → não)
    movl    $3,     %ebx        # divisor = 3
.primo_loop:
    movslq  %ebx,   %rax
    imulq   %rax,   %rax        # divisor² em 64 bits
    movslq  %edi,   %rdx
    cmpq    %rdx,   %rax        # divisor² > n → é primo
    jg      .primo_sim
    movl    %edi,   %eax        # dividendo (parte baixa)
    xorl    %edx,   %edx        # parte alta = 0 — divl divide EDX:EAX
    divl    %ebx                # eax = n/div,  edx = n%div
    testl   %edx,   %edx
    jz      .primo_nao          # resto 0 → composto
    addl    $2,     %ebx        # só divisores ímpares
    jmp     .primo_loop
.primo_ret:
    popq    %rbx                # devolve %rbx como estava
    ret
```

O laço precisa de um registrador que sobreviva à divisão, e `divl` destrói `%eax` e `%edx`. `%rbx`
serve — mas é callee-saved, então `pushq`/`popq` são obrigatórios. Repare que **todos** os caminhos de
saída passam por `.primo_ret`: um `ret` que pulasse o `popq` deixaria a pilha desequilibrada em 8 bytes,
e o `ret` do chamador saltaria para o valor de `%rbx`.

⚠ `divl %ebx` divide o par `EDX:EAX` (64 bits) por `%ebx`. Esquecer o `xorl %edx,%edx` antes não dá
"resultado errado": dá `#DE`, exceção que vira `SIGFPE` quando o quociente não cabe em 32 bits.

⚠ **Overflow corrigido aqui.** O código original fazia `imull %ebx, %ebx` — `divisor²` em 32 bits. Para
`n` perto de `INT_MAX`, o divisor chega a ~46341 e `46341² = 2147488281` transborda o `int`, virando
negativo; o teste `divisor² > n` nunca dispara na hora certa. Medido:

```
versão antiga (imull, 32 bits):  primo(2147483647) = 0   ← ERRADO
versão corrigida (imulq, 64):    primo(2147483647) = 1   ✓
```

2147483647 é `2³¹ − 1`, o oitavo primo de Mersenne, e a versão antiga o declarava composto.

## Como validar

```bash
make configure                              # apenas na primeira vez
make build
./dist/bin/outline_asm
./dist/bin/outline_asm | grep -c FALHOU     # deve imprimir 0
```

### À mão, para ver a linkagem acontecer

Sem Meson e sem Conan — este é o exercício que fecha o conceito da seção 1:

```bash
gcc -c src/outline_asm/asm_funcs.s -o /tmp/asm_funcs.o      # 1. assembla
nm /tmp/asm_funcs.o | grep ' T '                            # 2. o que ele EXPORTA
g++ -std=c++20 -c src/outline_asm/main.cpp -Isrc/outline_asm -o /tmp/main.o
nm -u /tmp/main.o | grep asm_                               # 3. o que o C++ PROCURA
```

O passo 2 lista os dez símbolos — `asm_eh_primo`, `asm_fatorial`, `asm_maximo`, `asm_memcpy_64`,
`asm_popcount`, `asm_soma_array`, `asm_soma_int`, `asm_soma_int64`, `asm_strlen`, `asm_valor_absoluto` —
e cada `U` do passo 3 tem um `T` de mesmo nome no passo 2. É por isso que o link funciona. Apague um
`.globl` do `.s` e o `T` correspondente vira `t` minúsculo: o link passa a falhar com
`undefined reference`.

### Ver o código de máquina que saiu

```console
$ objdump -d --no-show-raw-insn dist/bin/outline_asm | sed -n '/<asm_strlen>:/,/^$/p'
0000000000006b7b <asm_strlen>:
    6b7b:  xor    %eax,%eax
    6b7d:  mov    $0xffffffffffffffff,%rcx
    6b84:  repnz scas %es:(%rdi),%al
    6b86:  not    %rcx
    6b89:  lea    -0x1(%rcx),%rax
    6b8d:  ret

$ readelf -lW dist/bin/outline_asm | grep GNU_STACK    # espera-se "RW", nunca "RWE"
```

Seis instruções no binário final, exatamente as seis do `.s` — nenhum otimizador as tocou. É essa a
diferença entre escrever assembly e escrever C++.

## Referências

Subprojetos irmãos:

- [`../inline_asm/`](../inline_asm/) — o passo anterior: `asm(...)` dentro do C++, com constraints e
  clobbers.
- [`../asm_course/`](../asm_course/) — quinze módulos de assembly do zero; os módulos 6 e 7 cobrem
  *stack frames* e a convenção de chamada a fundo, inclusive o bug real de alinhamento de 16 bytes.
- [`../linkage/`](../linkage/) — o lado C++ da mesma moeda: unidades de tradução, `inline`, `static`,
  ODR e o que o linker faz com cada um.
- [`../layout/`](../layout/) — como os bytes de um objeto ficam arrumados, que é o que o assembly
  enxerga ao receber um ponteiro.

Fontes externas canônicas:

- **System V ABI, AMD64 Supplement** — <https://gitlab.com/x86-psABIs/x86-64-ABI>: o documento normativo
  da ABI descrita acima.
- **Using as, the GNU Assembler** — <https://sourceware.org/binutils/docs/as/>: referência das diretivas
  (`.globl`, `.type`, `.size`, `.section`).
- **Intel SDM, Vol. 2** — <https://www.intel.com/sdm>: definição de cada instrução (`scasb`, `movsq`,
  `cmov`, `popcnt`, `div`).
- **GCC: Extended Asm** — <https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html>: a alternativa inline,
  para comparar.

Dependências: `fmt` (formatação da saída) e o GAS, incluso no pacote `binutils`.
