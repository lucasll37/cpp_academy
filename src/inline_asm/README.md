# inline_asm — assembly escrito dentro do C++ (x86-64, sintaxe AT&T)

## O que é este projeto?

Doze demonstrações progressivas de **assembly inline**: instruções de máquina escritas no meio de
uma função C++, com o compilador ligando as duas coisas. Vai do `nop` que não faz nada até ler o
fabricante da CPU (`CPUID`) e o contador de ciclos do processador (`RDTSC`).

```bash
make build              # compila o repositório inteiro
./dist/bin/inline_asm   # roda as 12 demos, em ordem
```

Trecho da saída real:

```
══ 2. Output — asm escreve em variável C++ ══
  movl $42, %0  →  resultado = 42
  movl $99, %%eax  →  valor_eax = 99

══ 8. CPUID — lendo informações da CPU ══
  CPUID leaf 0:
    eax (max leaf) = 13
    vendor string  = "AuthenticAMD"
```

**Requer** x86-64 Linux com GCC ou Clang. As demos usam `fmt` só para imprimir.

---

## Antes de começar: os oito conceitos que o resto do texto usa

Não pressupõe nada além de C++ básico. Se você nunca viu assembly, comece por
[`../asm_course/`](../asm_course/) — mas esta seção é autossuficiente.

### 1. O que é, de fato, "assembly inline"

O GCC compila o seu C++ para assembly antes de virar binário. Um bloco `asm(...)` é **um pedaço
de texto que o GCC copia, literalmente, para dentro desse assembly que ele mesmo gerou** — no
ponto exato em que o bloco aparece no código.

```
   seu C++                    o assembly que sai do GCC
 ┌──────────────────┐       ┌─────────────────────────────┐
 │ int x = 7;       │ ────► │ movl   $7, -4(%rbp)         │  ← gerado pelo GCC
 │ asm("nop");      │ ────► │ nop                         │  ← SEU TEXTO, colado
 │ usa(x);          │ ────► │ call   _Z3usai              │  ← gerado pelo GCC
 └──────────────────┘       └─────────────────────────────┘
```

O compilador **não entende** o que você escreveu ali dentro: para ele é uma string opaca, e quem
vai interpretá-la é o **montador** (`as`), depois. Por isso um erro de digitação no template
aparece como "Assembler messages", nunca como erro de C++. E aí surge o problema central: se o
GCC não entende o texto, **como as suas variáveis C++ chegam lá dentro?** É para isso que existem
os três campos depois do template — saídas, entradas e *clobbers*. São a ponte, e são o assunto
dos itens 5 a 7.

### 2. O que é um registrador

O processador não sabe somar dois números que estão na RAM. Ele só opera sobre **registradores**:
um punhado de gavetas de 64 bits **dentro** do próprio processador, cada uma com nome próprio.

```
memória ──(load)──▶ registrador ──(operação)──▶ registrador ──(store)──▶ memória
```

O x86-64 tem 16 de uso geral: `RAX RBX RCX RDX RSI RDI RBP RSP R8 … R15` — **128 bytes no total**
contra bilhões de RAM, e essa escassez explica metade do assembly que você vai ler. Ler um
registrador custa ~0 ciclos; o cache L1, ~4; a RAM, centenas.

Cada registrador tem **quatro nomes**, um por largura, e todos são **a mesma célula física**:

```
┌───────────────────────── RAX (64 bits) ─────────────────────────┐
                                 ┌──────── EAX (32 bits) ─────────┐
                                                 ┌── AX (16) ─────┐
                                                        ┌─ AL(8) ─┐
```

> ⚠ Escrever em `EAX` **zera** os 32 bits de cima de `RAX`. Escrever em `AX` ou `AL` **não zera
> nada** — os bytes de cima continuam com o que havia antes. É a origem exata do bug da Demo 11.

### 3. As flags: `EFLAGS`, `ZF` e companhia

Existe um registrador especial — `EFLAGS` em 32 bits, `RFLAGS` em 64 — cujos bits o processador
atualiza como **efeito colateral** de quase toda operação aritmética ou lógica:

| flag | nome | liga quando |
|---|---|---|
| `ZF` | Zero | o resultado deu zero |
| `SF` | Sign | o resultado é negativo |
| `CF` | Carry | houve "vai um" além do bit mais alto (sem sinal) |
| `OF` | Overflow | o resultado estourou a faixa **com sinal** |

Duas instruções existem só para mexer nas flags e **jogar o resultado fora**:
- `cmp b, a` faz `a − b` e guarda só as flags. Se `ZF=1`, é porque `a == b`.
- `test b, a` faz `a AND b` e guarda só as flags. `test $1, %eax` liga `ZF` quando o bit 0 é
  zero — ou seja, quando o número é **par**. É a Demo 11.

Todo `if` em assembly é um `cmp`/`test` seguido de um salto condicional (`je`, `jl`, `jge`…) que
lê as flags. Como o seu bloco `asm` altera as flags e o GCC não tem como saber disso, você
**precisa** declarar o clobber `"cc"` (item 7).

### 4. Sintaxe AT&T (a do GCC no Linux) contra Intel

| Aspecto | AT&T (GCC/Linux, este projeto) | Intel (NASM, MSVC, manuais) |
|---|---|---|
| Ordem dos operandos | `origem, destino` | `destino, origem` |
| Registradores | `%rax`, `%ebx` | `rax`, `ebx` |
| **Imediato** (constante literal no código) | `$42`, `$1` | `42`, `1` |
| Sufixo de tamanho | na instrução: `movb movw movl movq` | no operando: `mov byte ptr [x], 1` |
| Desreferência | `(%rax)`, `4(%rax)` | `[rax]`, `[rax+4]` |

Um **imediato** é uma constante que viaja embutida na própria instrução, sem passar por memória
nem registrador. `movl $42, %eax` carrega o número 42; `movl 42, %eax` (sem o `$`) leria o
**endereço** 42. O cifrão não é enfeite.

O **sufixo de tamanho** é obrigatório em AT&T porque a sintaxe não diz a largura em lugar nenhum:

| sufixo | bits | exemplo |
|---|---|---|
| `b` | 8 | `movb`, `sete %al` |
| `w` | 16 | `movw` |
| `l` | 32 | `movl`, `addl`, `imull` |
| `q` | 64 | `movq`, `addq` |

Quer ver a mesma coisa em sintaxe Intel? `objdump -d -M intel ./dist/bin/inline_asm`.

### 5. O formato do `asm` estendido, e o que é uma *constraint*

```cpp
asm [volatile] (
    "template com %0, %1 …"   // 1) texto colado no assembly gerado
    : saídas                  // 2) [opcional] variáveis C++ que o bloco ESCREVE
    : entradas                // 3) [opcional] variáveis C++ que o bloco LÊ
    : clobbers                // 4) [opcional] o que mais o bloco estraga
);
```

Cada operando das listas 2 e 3 tem a forma `"constraint"(variável)`.

Uma **constraint** é um contrato com o **alocador de registradores** do GCC: diz **onde este
operando pode viver** para o seu texto conseguir usá-lo. O GCC lê a constraint, escolhe um lugar
concreto, emite o código que põe a variável lá antes do bloco (e a tira depois), e substitui
`%0`, `%1`… pelo nome do lugar escolhido.

| Constraint | "este operando pode viver…" |
|---|---|
| `"r"` | em qualquer registrador de uso geral |
| `"a"` `"b"` `"c"` `"d"` | obrigatoriamente em `rax`/`rbx`/`rcx`/`rdx` (e seus aliases) |
| `"q"` | num registrador que **tenha forma de 8 bits** (`al`, `bl`, `cl`, `dl`) |
| `"m"` | na memória — o GCC gera o modo de endereçamento |
| `"i"` | como imediato: precisa ser constante em tempo de compilação |
| `"0"`, `"1"`… | **no mesmo lugar** que o operando de número 0, 1… |

E os prefixos que só valem nas **saídas**:

| Prefixo | Significado |
|---|---|
| `"="` | **write-only**: o bloco escreve, e o GCC **não** garante o valor anterior |
| `"+"` | **read-write**: o bloco lê e escreve o mesmo lugar |
| `"=&"` | *early clobber*: o bloco escreve na saída **antes** de terminar de ler as entradas |

> ⚠ O `&` do `"=&r"` não é detalhe. Sem ele, o GCC pode escolher **o mesmo registrador** para uma
> saída e uma entrada — o que é perfeitamente legal, já que ele supõe que você só escreve na
> saída depois de ler tudo. Se o seu template escrever antes, a entrada é destruída. A Demo 6.5
> mostra o caso.

### 6. A numeração dos operandos — onde todo iniciante erra

Os operandos são numerados numa **sequência única**, percorrendo **primeiro todas as saídas** e
**depois todas as entradas**, na ordem em que aparecem. Clobbers não recebem número.

```
asm ( "…%0…%1…%2…%3…"
      : "=r"(A), "=r"(B)     ← saídas   →  %0 = A   %1 = B
      : "r"(C),  "r"(D)      ← entradas →  %2 = C   %3 = D
      : "cc" );              ← clobbers →  sem número
```

Aplicado à Demo 5 deste projeto, que tem exatamente duas saídas e duas entradas:

```cpp
asm( "movl %2, %%eax\n\t"     //  %2 = x        (1ª entrada)
     "movl %3, %%ebx\n\t"     //  %3 = y        (2ª entrada)
     "xchgl %%eax, %%ebx\n\t"
     "movl %%eax, %0\n\t"     //  %0 = troca_a  (1ª saída)
     "movl %%ebx, %1"         //  %1 = troca_b  (2ª saída)
     : "=r"(troca_a), "=r"(troca_b)
     : "r"(x), "r"(y)
     : "%eax", "%ebx" );
```

> ⚠ Acrescentar uma saída **renumera todas as entradas**. É por isso que a Demo 12 existe: com
> operandos nomeados (`%[largura]`), inserir um operando não quebra o resto do bloco.

Outra armadilha de escrita: dentro do template, `%` é o caractere de escape do próprio GCC. Para
escrever um registrador literal você precisa de **dois**: `%%eax`. Um `%` sozinho seguido de
dígito é referência a operando.

### 7. O que é um *clobber*

O GCC precisa saber o que o seu bloco estraga, porque ele mantém valores vivos em registradores
entre uma linha e outra do seu C++. **Ele assume que o bloco não toca em nada além do que foi
declarado.** Se você usa `%eax` diretamente no template sem declará-lo, e o GCC estava guardando
uma variável ali, essa variável é destruída — sem aviso, sem erro, sem sintoma imediato.

*Clobber* ("o que foi destroçado") é a terceira lista. Três formas:

| Clobber | Quer dizer |
|---|---|
| `"%eax"`, `"%ecx"`… | o bloco escreve nesse registrador por conta própria |
| `"cc"` | o bloco altera as **flags** (item 3) |
| `"memory"` | o bloco pode ler ou escrever memória arbitrária |

> ⚠ **Não se pode listar nos clobbers um registrador que já é operando.** O GCC recusa a colisão.
> Quando o bloco escreve num registrador que também é entrada, a forma correta é `"+"`
> (read-write), não clobber — é exatamente o bug de ABI documentado no módulo 10 de
> [`../asm_course/`](../asm_course/).
>
> A recíproca é útil: listar `"%eax"` no clobber **é seguro** mesmo com uma saída `"=r"`, porque
> o alocador **exclui** registradores clobbered da escolha dos operandos. `%0` nunca cairá em
> `%eax`. É o que faz a Demo 7 funcionar.

### 8. *Load*, *store* e o tal do "reordenamento"

Um **load** é ler da memória para um registrador; um **store** é o caminho inverso. O compilador
reordena livremente loads e stores que ele julga independentes — é uma das otimizações mais
lucrativas que existem, e ele o faz porque a semântica de uma thread só não muda.

Só que ela **muda** para outra thread, ou para um manipulador de sinal, ou para um registrador
mapeado em memória de um periférico. Daí a existência das **barreiras**, e daí a Demo 6.

> ⚠ Existem **duas** barreiras diferentes, e confundi-las é o erro conceitual mais caro do
> assunto:
> - **barreira de compilador** — proíbe o *compilador* de mover acessos através de um ponto.
>   **Não emite nenhuma instrução.** É o `asm volatile ("" ::: "memory")`.
> - **barreira de hardware** — emite uma instrução real (`mfence`, `lock …`) que impede a *CPU*
>   de reordenar. É o `std::atomic_thread_fence`.

---

## Por que estudar isto? E, principalmente, quando NÃO usar

| Situação | Sem assembly inline | Com |
|---|---|---|
| Instrução sem equivalente em C++ (`CPUID`, `RDTSC`, `RDRAND`) | impossível em C++ puro | quatro linhas |
| Impedir o compilador de reordenar acessos | `std::atomic_signal_fence` | `asm volatile("":::"memory")` — a mesma coisa |
| Ler o resultado de uma flag como inteiro | o compilador decide sozinho | `sete`/`setne` explícito |
| Entender o `-S` do seu próprio código | você confia ou reza | você lê |

**Quando não usar — que é quase sempre.** O compilador de 2026 é melhor que você em quase todo
caso, e o teste é objetivo. Escreva a versão em C++ puro e olhe o que sai:

```bash
printf 'int soma(int a, int b) { return a + b; }\n' > /tmp/s.cpp
g++ -O2 -S /tmp/s.cpp -o - | sed -n '/_Z4somaii:/,/ret/p' | grep -vE '^\s*\.|^$'
```

```
_Z4somaii:
	endbr64
	leal	(%rdi,%rsi), %eax     ← UMA instrução faz a soma
	ret
```

A Demo 4 deste projeto usa **duas** (`movl` + `addl`) para fazer o mesmo. O assembly inline
piorou o código, além de impedir o inlining e a vetorização. Três motivos concretos para preferir
C++:

1. o bloco `asm` é **opaco** ao otimizador — ele não pode dobrar constantes através dele, nem
   vetorizar um laço que o contenha;
2. **existem intrínsecos** para quase tudo o que motiva as Demos 8 e 9: `__get_cpuid()` de
   `<cpuid.h>`, `__rdtsc()` de `<x86intrin.h>`, `__builtin_cpu_supports("avx2")` (devolve
   não-zero se houver suporte) e a família `<immintrin.h>` para SIMD. São funções normais, que o
   otimizador **entende** — e portáveis entre GCC, Clang e ICC;
3. o site que resolve a dúvida em dez segundos é **godbolt.org**: cola-se o C++ de um lado e vê-se
   o assembly do outro, com as linhas coloridas por correspondência.

O valor real deste subprojeto é **ler**, não escrever: entender o bloco `asm` que você encontrou
num driver, num alocador ou num cabeçalho de biblioteca.

---

## Estrutura de arquivos

```
inline_asm/
├── main.cpp     ← as 12 demos, uma função cada, chamadas em ordem por main()
├── meson.build  ← alvo 'inline_asm', depende só de fmt
└── README.md    ← este arquivo
```

O parente próximo é [`../outline_asm/`](../outline_asm/), onde as funções em assembly ficam em
arquivos `.s` separados, montados pelo `as` e linkados ao C++: o passo seguinte natural.

---

## Os demos, um a um

### Demo 1 — `nop`: a forma mais curta que existe

```cpp
asm("nop");   // No Operation, opcode 0x90: consome um slot e não faz mais nada
```

Sem `:` nenhum, o bloco é a forma **básica** do `asm` — nem operandos, nem clobbers. Serve para
alinhamento, para *delays* em microcontroladores e como marca visível ao desmontar o binário.

### Demo 2 — uma saída: o asm escreve numa variável C++

```cpp
int resultado;
asm("movl $42, %0"      // $42 é imediato; %0 é o operando 0
    : "=r"(resultado)); // "=r": saída write-only, em qualquer registrador
// resultado == 42

int valor_eax;
asm("movl $99, %%eax"   // %% para escrever um % literal
    : "=a"(valor_eax)); // "=a": obrigatoriamente eax
// valor_eax == 99
```

Repare no que o GCC fez sozinho no primeiro caso: escolheu um registrador livre, colou o seu
`movl`, e depois emitiu o `mov` que leva o valor desse registrador para a variável `resultado`.
Você nunca soube qual registrador foi — e não precisa saber.

### Demo 3 — uma entrada: o asm lê uma variável C++

```cpp
int x = 7, saida;
asm("movl %1, %0"    // origem → destino (AT&T)
    : "=r"(saida)    // %0 — saída
    : "r"(x));       // %1 — entrada, sem "=" porque só é lida
// saida == 7
```

### Demo 4 — aritmética: `add`, `inc`, `imul`

```cpp
int a = 10, b = 25, soma;
asm("movl %1, %0\n\t"   // \n\t separa instruções: newline para o montador,
    "addl %2, %0"       // tab só para o assembly gerado ficar legível
    : "=r"(soma) : "r"(a), "r"(b));
// soma == 35

int n = 41;
asm("incl %0" : "+r"(n));   // "+" = lê E escreve o mesmo operando
// n == 42

int32_t fator = 6, multiplicando = 7, produto;
asm("imull %2, %0"
    : "=r"(produto)
    : "0"(fator),          // "0": entrada amarrada ao MESMO lugar do operando 0
      "r"(multiplicando));
// produto == 42
```

A amarração `"0"(fator)` é o idioma para instruções **destrutivas** — `imull src, dst` sobrescreve
`dst`. Ela diz: "ponha `fator` no mesmo registrador que a saída, porque a instrução vai comer o
destino". Sem isso o valor inicial de `%0` seria indefinido.

> ⚠ Faltou o clobber `"cc"` nos três blocos acima: `addl`, `incl` e `imull` **alteram as flags**.
> Funciona aqui porque o GCC não tinha nenhuma flag viva atravessando esses pontos. É a espécie
> de omissão que só quebra quando alguém edita a função em volta.

### Demo 5 — clobbers: registradores usados por fora dos operandos

O bloco da seção "numeração dos operandos" acima é este demo por inteiro. Ele usa `%eax` e `%ebx`
diretamente no template — nenhum dos dois é operando —, então os declara na terceira lista.
Resultado: `troca_a == 200`, `troca_b == 100`.

Logo abaixo, no mesmo demo, o uso mínimo de `"cc"`:

```cpp
int val = 5;
asm("subl $3, %0" : "+r"(val) : : "cc");   // subl mexe em ZF, SF, CF, OF
// val == 2
```

A lista de entradas fica **vazia** entre os dois `:` — sem ela, o terceiro `:` viraria entrada.

### Demo 6 — `volatile`: a regra é o inverso da intuição

Esta era a parte **errada** deste README, e vale ler devagar. Do manual do GCC (*Extended Asm*):

> Um `asm` **sem operandos de saída** já é **implicitamente `volatile`**: o GCC nunca o remove.
> Um `asm` **com** operandos de saída **pode ser removido** se o GCC concluir que nenhuma dessas
> saídas é usada.

Ou seja: `volatile` **não** serve para o caso sem saídas (ali é redundante). Ele serve
exatamente para o caso **com** saídas — impedindo que o bloco seja apagado, movido para fora de
um laço, ou executado uma vez só. A prova cabe em três funções:

```cpp
void a() { asm("nop\n\tnop\n\tnop"); }                            // sem saída
void b() { int r; asm("nop\n\tnop\n\tnop" : "=r"(r)); (void)r; }  // com saída
void c() { int r; asm volatile("nop\n\tnop\n\tnop" : "=r"(r)); (void)r; }
```

Salve as três em `/tmp/vol.cpp` e conte os `nop` que sobraram em cada uma:

```bash
g++ -O2 -S /tmp/vol.cpp -o /tmp/vol.s
awk '/^_Z1[abc]v:/{f=$0} /nop/{c[f]++} END{for (k in c) print k, c[k]}' /tmp/vol.s
```

```
_Z1av: 3      ← ficaram: sem saída = implicitamente volatile
_Z1cv: 3      ← ficaram: volatile explícito
              ← _Z1bv não aparece: os três NOPs SUMIRAM
```

A segunda metade do demo é a barreira:

```cpp
asm volatile ("" ::: "memory");
```

> ⚠ Isto é uma **barreira de compilador**, não "uma barreira de memória completa". O template é
> a string vazia: **nenhuma instrução é emitida** — confira com `-S`. O que o bloco faz é
> proibir o GCC de mover loads e stores através desta linha. A **CPU continua livre para
> reordenar**.
>
> Ela é **exatamente equivalente** a `std::atomic_signal_fence(std::memory_order_seq_cst)` — não
> "mais forte". O análogo de hardware é `std::atomic_thread_fence(std::memory_order_seq_cst)`,
> que no x86-64 emite `lock orq $0, (%rsp)`. Escolher a errada é como pôr cadeado numa porta e
> deixar a janela aberta; o assunto está em [`../concurrency/`](../concurrency/).

### Demo 6.5 — um `if` em assembly (o degrau que falta antes do laço)

Este não está no `main.cpp`: é para você digitar, compilar e rodar antes de encarar a Demo 7,
porque ela junta três conceitos novos de uma vez. Aqui vem só um: o salto condicional.

```cpp
static int maximo(int a, int b) {
    int r;
    asm ("movl %1, %0\n\t"    // r = a
         "cmpl %0, %2\n\t"    // calcula b - r  → ordem INVERTIDA em AT&T
         "jle  fim%=\n\t"     // se b <= r, pula o próximo movl
         "movl %2, %0\n\t"    // r = b
         "fim%=:"             // %= gera um número único por instância do bloco
         : "=&r"(r)           // & porque escrevemos %0 ANTES de ler %2
         : "r"(a), "r"(b)
         : "cc");             // cmpl mexe nas flags
    return r;
}
// maximo(3,9) == 9   maximo(9,3) == 9   maximo(-4,-7) == -4
```

Três coisas para levar:

- ⚠ **`cmpl %2, %%ecx` calcula `ecx − %2`** — a ordem é invertida em relação à leitura natural.
  É a pegadinha nº 1 da sintaxe AT&T. `jle` depois disso salta quando o **segundo** operando
  escrito é menor ou igual ao primeiro.
- **`%=`** gera um número único por expansão do bloco. Sem ele, se a função for inlinada duas
  vezes, o montador vê o label `fim` duplicado e recusa.
- **`"=&r"`** é obrigatório aqui: escrevemos em `%0` antes de ler `%2`. Sem o `&`, o GCC teria
  permissão para colocar os dois no mesmo registrador e o valor de `b` seria destruído.

### Demo 7 — um laço inteiro em assembly

```cpp
int N = 10, acumulador = 0;
asm volatile (
    "xorl %%ecx, %%ecx\n\t"       // ecx = 0  (contador i) — xor consigo mesmo é o
    "xorl %%eax, %%eax\n\t"       // eax = 0  (soma)         idioma de "zere isto"
    "loop_start%=:\n\t"
    "  incl %%ecx\n\t"            // ++i
    "  addl %%ecx, %%eax\n\t"     // soma += i
    "  cmpl %1, %%ecx\n\t"        // calcula ecx - N
    "  jl   loop_start%=\n\t"     // se i < N, repete
    "movl %%eax, %0"
    : "=r"(acumulador)            // %0
    : "r"(N)                      // %1
    : "%eax", "%ecx", "cc");
// acumulador == 55  (10*11/2)
```

`xorl %%eax, %%eax` é como se zera um registrador em x86: a codificação é menor que a de
`movl $0, %eax` e o processador reconhece o padrão. O laço é a tradução direta de
`for (i = 1; i <= N; ++i) soma += i;`.

> ⚠ A versão anterior deste demo listava uma entrada extra, `"0"(acumulador)`, amarrada à saída
> — **e o valor nunca era lido**, porque a primeira instrução do bloco zera `eax`. Amarração
> inútil ensina padrão errado, e foi removida.
>
> E note por que declarar `"%eax"` no clobber é seguro mesmo com a saída `"=r"`: o alocador
> exclui registradores clobbered da escolha dos operandos, então `%0` jamais será `eax`.

### Demo 8 — `CPUID`: perguntar à CPU quem ela é

```cpp
uint32_t eax, ebx, ecx, edx;
asm volatile ("cpuid"
    : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)   // quatro saídas fixas
    : "a"(0));                                      // entrada: leaf 0
```

Um **leaf** ("folha") é o número que se põe em `eax` **antes** de executar `CPUID` para escolher
*qual pergunta* está sendo feita. Leaf 0 devolve o maior leaf suportado (aqui: 13) e a string de
12 caracteres do fabricante, distribuída em `ebx`, `edx` e `ecx` — nessa ordem esquisita:

```cpp
char vendor[13] = {};
__builtin_memcpy(vendor + 0, &ebx, 4);
__builtin_memcpy(vendor + 4, &edx, 4);
__builtin_memcpy(vendor + 8, &ecx, 4);
// "AuthenticAMD" nesta máquina; "GenuineIntel" numa Intel
```

Este é o demo que **justifica** o assembly inline: `cpuid` não tem equivalente em C++ padrão e
precisa de quatro registradores fixos simultâneos. Ainda assim, ⚠ `<cpuid.h>` já traz
`__get_cpuid(0, &a, &b, &c, &d)`, que faz o mesmo e trata leaf não suportado.

### Demo 9 — `RDTSC`: o contador de ciclos

```cpp
uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));   // resultado sai em edx:eax
    return ((uint64_t)hi << 32) | lo;              // recompõe os 64 bits
}
```

O **TSC** (*Time-Stamp Counter*) é um contador de 64 bits que a CPU incrementa continuamente
desde o boot. A instrução entrega os 32 bits baixos em `eax` e os altos em `edx`; o C++ recombina
os dois. No `main.cpp` ele mede um laço de 1000 somas: 1700–2500 ciclos, variando a cada execução.

> ⚠ **O TSC não é um relógio confiável para medir código.** Quatro razões:
> 1. o processador **executa fora de ordem** — `rdtsc` pode ser executada antes ou depois do que
>    você queria medir. Medições sérias usam `rdtscp` ou um `lfence` antes;
> 2. o TSC conta numa frequência **fixa**, enquanto a frequência real do núcleo sobe e desce
>    (*frequency scaling*), então "ciclos" não vira "tempo" por uma constante;
> 3. se o SO migrar sua thread para outro núcleo, o contador pode dar um salto;
> 4. dentro de máquina virtual ou container, a instrução pode ser interceptada.
>
> Para medir tempo, use `std::chrono::steady_clock` — é o que
> [`../performance/`](../performance/) faz. Para medir ciclos com rigor, `perf stat`.

### Demo 10 — a constraint `"m"`: operando na memória

```cpp
int arr[4] = {1, 2, 3, 4};
int resultado;
asm volatile (
    "movl  %1,    %%eax\n\t"   // eax = arr[0]
    "addl  4%1,   %%eax\n\t"   // eax += arr[1]   ← concatenação de TEXTO
    "addl  8%1,   %%eax\n\t"
    "addl  12%1,  %%eax\n\t"
    "movl  %%eax, %0"
    : "=r"(resultado)
    : "m"(*arr)
    : "%eax", "cc");
// resultado == 10
```

> ⚠ **`4%1` é um truque frágil, não uma técnica.** Não existe aritmética de endereços no template
> — é **concatenação de texto**. Funciona porque, para um array **local**, o GCC expande `%1`
> como `(%rsp)`, e `"4"` + `"(%rsp)"` = `4(%rsp)`, que é assembly válido.
>
> Troque `arr` por um array **global** e o GCC expande `%1` como `glob(%rip)`. A concatenação
> produz `4glob(%rip)` e o montador recusa — verificado:
>
> ```
> Assembler messages: Error: junk `glob' after expression
> ```
>
> A forma robusta é passar o **endereço** num registrador e usar deslocamento de verdade:
>
> ```cpp
> asm ("movl  (%1), %%eax\n\t"
>      "addl 4(%1), %%eax\n\t"
>      "movl %%eax, %0"
>      : "=r"(r) : "r"(arr) : "%eax", "cc");
> ```

### Demo 11 — `test` + `sete`, e a armadilha dos 8 bits

Padrão clássico: transformar o resultado de uma instrução que só mexe em flags num valor inteiro
de C++. `SETcc` escreve **1** num registrador de **8 bits** quando a condição vale, e **0** caso
contrário. `sete` testa `ZF=1`.

```cpp
auto eh_par = [](int n) -> bool {
    unsigned char b;              // 8 bits: exatamente o que SETE escreve
    asm ("testl $1, %1\n\t"       // n & 1 → ZF=1 se par
         "sete  %0"               // %0 tem 8 bits: b = 1 se ZF=1
         : "=q"(b)                // "q": registrador com forma de 8 bits (al/bl/cl/dl)
         : "r"(n)
         : "cc");
    return b != 0;
};
```

> ⚠ **A versão anterior deste demo estava errada, e foi corrigida no `main.cpp`.** Era assim:
>
> ```cpp
> int resultado;                  // NÃO inicializado, 32 bits
> asm ("testl $1, %1\n\t"
>      "sete  %b0"                // escreve SÓ o byte baixo
>      : "=r"(resultado) : "r"(n) : "cc");
> return resultado;               // os 3 bytes altos são LIXO
> ```
>
> `"=r"` é **write-only**: o GCC não preserva nem zera o que já havia no registrador (veja o
> item 2 — escrever em `AL` não zera `EAX`). Os 24 bits altos ficam indeterminados. Compilado e
> executado de verdade, com o mesmo fonte:
>
> ```
> -O0:  eh_par_bugado(0) = 1            ← "funciona"
> -O2:  eh_par_bugado(0) = 1691106561   ← lixo; e o valor muda a cada build
> ```
>
> `if (eh_par(n))` daria "verdadeiro" para esse lixo. A correção é declarar a variável com o
> **tamanho da instrução** (`unsigned char` + `"=q"`), como está acima. A alternativa é zerar
> antes com `"xorl %0, %0"` e uma constraint `"=&r"`.

### Demo 12 — operandos nomeados

```cpp
int largura = 16, altura = 9, area;
asm ("movl  %[w], %[out]\n\t"
     "imull %[h], %[out]"
     : [out] "=r"(area)
     : [w] "r"(largura), [h] "r"(altura));
// area == 144
```

Em blocos com mais de três operandos, nomes eliminam a classe inteira de bugs de renumeração do
item 6. Use por padrão.

---

## Como validar

**1 · Compila e roda.**

```bash
make build && ./dist/bin/inline_asm; echo "saiu com $?"   # esperado: 0
```

Ao fim das doze seções o programa imprime `✓ Todas as demos executadas`.


**2 · Compila isolado, sem warning.** O único requisito externo é o `fmt`, cujo `.pc` o
`make configure` deixa em `build/`:

```bash
PKG_CONFIG_PATH=$PWD/build g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
    src/inline_asm/main.cpp -o /tmp/ia_O2 $(PKG_CONFIG_PATH=$PWD/build pkg-config --cflags --libs fmt)
```

Estado atual: **compila limpo em `-O0` e em `-O2`**.

**3 · O teste que importa: compare `-O0` com `-O2`.** É o único jeito de o leitor perceber
sozinho os problemas de assembly inline — bloco removido, byte alto com lixo, entrada destruída.
Tudo isso aparece na diferença entre os dois níveis:

```bash
PKG_CONFIG_PATH=$PWD/build g++ -std=c++20 -O0 -Wall -Wextra src/inline_asm/main.cpp \
    -o /tmp/ia_O0 $(PKG_CONFIG_PATH=$PWD/build pkg-config --cflags --libs fmt)
diff <(/tmp/ia_O0 | grep -vE 'TSC|Ciclos') <(/tmp/ia_O2 | grep -vE 'TSC|Ciclos') \
    && echo "IDENTICO — nenhum demo depende do nível de otimização"
```

As linhas de TSC são excluídas porque mudam a cada execução, por natureza.


**4 · Veja o assembly de verdade.**

```bash
g++ -O2 -S src/inline_asm/main.cpp -o - | less     # com o seu texto colado no meio
objdump -d -M intel ./dist/bin/inline_asm | less   # já montado, em sintaxe Intel
gdb -batch -ex 'disassemble demo_loop_asm' ./dist/bin/inline_asm
```

Procure pelos três `nop` da Demo 6 e pelo `loop_start` da Demo 7: eles estão lá, com o número
gerado pelo `%=` no lugar.

---

## Referências

Neste repositório:
- [`../asm_course/`](../asm_course/) — os quinze módulos de fundamento (registradores, pilha,
  flags, ABI, SIMD). Comece por aqui se algo do item 2 ou 3 foi novidade
- [`../outline_asm/`](../outline_asm/) — o passo seguinte: assembly em arquivos `.s` separados,
  *name mangling*, `extern "C"` e a convenção de chamada
- [`../bits/`](../bits/) — as mesmas operações (`test`, máscaras, `popcount`) do lado do C++
- [`../concurrency/`](../concurrency/) — barreiras de verdade: `std::atomic_thread_fence` e as
  ordens de memória
- [`../performance/`](../performance/) — como medir sem `rdtsc`, com metodologia

Fora dele:
- **GCC manual, capítulo *Extended Asm*** — a referência normativa de constraints, modificadores
  (`%b0`, `%w0`, `%k0`) e da regra do `volatile`. É curto e vale ler inteiro
- **godbolt.org** (Compiler Explorer) — fonte e assembly lado a lado, coloridos por
  correspondência. O jeito mais rápido de descobrir que você não precisa de assembly inline
- *Intel 64 and IA-32 Software Developer's Manual* — o que cada instrução faz e que flags altera
