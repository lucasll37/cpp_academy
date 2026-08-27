# linkage — Unidades de Tradução, ODR, Linkage e ABI

## O que é este projeto?

O C++ não compila "o projeto". Ele compila **uma unidade de tradução por vez**, em total
ignorância das outras:

```
uma TU  =  um .cpp  +  tudo que os #include dele trouxeram, recursivamente
```

Cada TU vira um `.o` com uma lista de símbolos **definidos** e outra de símbolos
**necessários**. O linker cola tudo no fim. Praticamente todo erro confuso de build —
`undefined reference`, `multiple definition`, `undefined reference to vtable` — é consequência
direta desse modelo.

**Este é o único subprojeto do repositório com múltiplas unidades de tradução**, e não por
capricho: o assunto é exatamente o que acontece quando o mesmo cabeçalho é compilado mais de
uma vez.

| arquivo | papel |
|---|---|
| `unidade_a.cpp` / `unidade_b.cpp` | idênticos a menos do nome — provam a ODR |
| `comum.hpp` | o cabeçalho incluído pelos dois; o palco do experimento |
| `c_api.c` | compilado pelo **compilador C** (`cc`), não pelo `c++` |
| `widget.cpp` / `widget.hpp` | a metade escondida do pimpl |

### O experimento, inteiro, em 20 linhas

Não há truque nenhum nos números deste README. É este cabeçalho, incluído por dois `.cpp`:

```cpp
// comum.hpp — incluído por unidade_a.cpp E por unidade_b.cpp
#pragma once

namespace comum {

inline int funcao_inline() { return 1; }     // UMA no programa
static int funcao_static() { return 2; }     // uma POR TU
namespace { int funcao_anonima() { return 3; } }   // uma POR TU

inline int contador_inline = 0;              // UM objeto   (C++17)
static int contador_static = 0;              // um POR TU   ⚠

const     int kConstante        = 42;        // interna em C++ (externa em C)
constexpr int kConstexpr        = 43;        // interna (constexpr implica const)
inline constexpr int kConstexprInline = 44;  // EXTERNA

template <typename T> T identidade(T v) { return v; }   // fundida pelo linker

// declaradas aqui, DEFINIDAS uma em cada TU:
void incrementa_em_a(int n);
void incrementa_em_b(int n);
int  contador_inline_visto_de_a();
int  contador_static_visto_de_a();
// … e os equivalentes de b

}  // namespace comum
```

E este `.cpp` — `unidade_b.cpp` é **idêntico** a menos da letra no nome:

```cpp
// unidade_a.cpp
#include "comum.hpp"

namespace comum {

void incrementa_em_a(int n) {
    for (int i = 0; i < n; ++i) { ++contador_inline; ++contador_static; }
}

int contador_inline_visto_de_a() { return contador_inline; }
int contador_static_visto_de_a() { return contador_static; }

}  // namespace comum
```

O `main.cpp` chama `incrementa_em_a(10)` e `incrementa_em_b(10)` e depois pergunta a cada TU
quanto ela vê. As tabelas do demo 2 são a resposta. Os endereços e contadores impressos vêm do
**linker**, não de simulação.

```bash
make build && ./dist/bin/linkage
```

## Antes de começar

Este README não pressupõe nada além de já ter compilado um programa em C++ com mais de um
arquivo. Todo termo usado adiante está definido aqui. Se algum já for familiar, pule.

### Declarar não é definir — a distinção que sustenta o subprojeto inteiro

Se você levar **uma** ideia deste README, que seja esta:

| | o que é | pode aparecer | exemplos |
|---|---|---|---|
| **declaração** | apresenta o **nome** e o **tipo** de algo, sem dizer o que ele é | quantas vezes quiser, em quantas TUs quiser | `int f(int);` · `extern int x;` · `class Widget;` |
| **definição** | dá o **corpo** da função, ou **reserva o armazenamento** da variável | uma vez só (com exceções, adiante) | `int f(int a){return a;}` · `int x;` · `class Widget { … };` |

O **compilador** só precisa de declarações: para gerar `chama f com este inteiro` ele basta
saber o tipo. O **linker** é quem precisa de definições: no fim, alguém tem que ter escrito o
corpo de `f` em algum lugar. Daí a regra de diagnóstico mais útil que existe em C++:

> **erro do compilador → falta uma DECLARAÇÃO.**
> **erro do linker → falta (ou sobra) uma DEFINIÇÃO.**

Toda a tabela de erros do demo 1, a ODR inteira do demo 2 e a seção `extern` do demo 3 são
consequências dessa única distinção.

Cuidado com uma armadilha de vocabulário: `int x;` em escopo de namespace é uma **definição**
(reserva os 4 bytes). Só com `extern` (`extern int x;`) ela vira declaração. E `class Widget {
… };` é a **definição** da classe — `class Widget;` é a declaração, chamada de *declaração
adiantada* (ou *forward declaration*): "existe um tipo com este nome, ainda não sei como ele é
por dentro". Com ela dá para usar `Widget*` e `Widget&`, mas não `Widget` por valor, nem chamar
métodos, nem saber o `sizeof` — o tipo está **incompleto**.

### O vocabulário mínimo

**TU — unidade de tradução (*translation unit*).** Um `.cpp` depois que o pré-processador colou
nele, recursivamente, tudo que os `#include` trouxeram. É a unidade que o compilador enxerga —
o resto do projeto simplesmente não existe para ele.

**Símbolo.** O nome que sobrevive no arquivo objeto (`.o`) para identificar uma função ou
variável **definida** ali, ou **necessária** dali. `nm arq.o` lista todos: `T` = definido no
texto (código), `D`/`B` = dado definido, `U` = **undefined**, isto é, precisa de outra TU.

**Linker.** O programa (`ld`) que junta os `.o` e resolve cada `U` de um com um `T` de outro.
Ele **não entende C++**: só casa strings de símbolo. Metade das lições deste subprojeto vem
dessa cegueira.

**Name mangling.** A codificação que o compilador C++ aplica ao nome — `soma(int,int)` vira
`_Z4somaii` — para caber num formato de objeto que só tem strings e ainda distinguir
sobrecargas, namespaces e templates. Assunto do demo 4.

**Linkage.** A propriedade de um nome que responde: *este nome pode se referir à mesma entidade
que um nome igual em outra TU?* **Externa** = sim; **interna** = não, cada TU tem a sua;
**nenhuma** = a pergunta nem se coloca. Assunto do demo 3.

**ODR — *One Definition Rule*, regra da definição única.** As três regras do demo 2 sobre
quantas vezes cada coisa pode ser definida.

**UB — *undefined behavior*, comportamento indefinido.** A situação em que o padrão **não impõe
nada**: pode funcionar, pode travar, e o otimizador tem permissão para **assumir que ela nunca
acontece**. Não é "erro em runtime" — é a ausência de contrato. Ver
[`src/undefined_behavior/`](../undefined_behavior/).

**Weak symbol e COMDAT.** O mecanismo com que o linker **funde** as cópias de uma função
`inline` ou de um template instanciado em várias TUs. Cada cópia vai para uma seção COMDAT
marcada com uma chave (o nome manglado); o linker mantém **uma** e descarta as outras, sem
reclamar de duplicata. É por isso que `inline` não dá `multiple definition` — e também por que
uma violação de ODR passa despercebida: ele mantém uma **arbitrariamente**, sem comparar.

**vtable e vptr.** Cada classe com método `virtual` ganha **um** array de ponteiros de função
no binário (a *vtable*, uma por classe) e cada objeto ganha um ponteiro para ela (o *vptr*).
A **posição** de cada método nesse array é decidida em compilação. Isso é medido em
[`src/layout/`](../layout/), e é a base do argumento do demo 5 sobre quebra de ABI.

**Key function.** A **primeira** função virtual não-inline e não-pura de uma classe. O GCC e o
Clang emitem a vtable inteira na TU que **define** essa função — uma só, em vez de uma cópia em
cada TU. Se ela nunca for definida, a vtable não é emitida em lugar nenhum, e o erro é
`undefined reference to vtable for X`.

**Unwinding (desenrolar da pilha).** O mecanismo que, quando uma exceção é lançada, percorre os
quadros de chamada de volta rodando **os destrutores** de cada objeto local até achar o `catch`.
Ele não é adivinhação: depende de **tabelas** que o compilador emite junto com o código. O
compilador **C** não gera essas tabelas — daí a regra do demo 4 de que exceções não atravessam
uma fronteira `extern "C"` chamada a partir de C.

**Convenção de chamada (*calling convention*).** O acordo, dentro da ABI, sobre **como** uma
chamada acontece no nível de máquina: quais argumentos vão em quais registradores e quais na
pilha, em que registrador volta o retorno, quem limpa a pilha depois, quais registradores a
função pode destruir. No x86-64 SysV, por exemplo, os seis primeiros inteiros vão em `rdi,
rsi, rdx, rcx, r8, r9` e o retorno em `rax`. Chamador e chamado precisam concordar; discordar
é corrupção silenciosa.

**API × ABI.** **API** é o contrato de **código-fonte**: se ele quebra, você **recompila** e
resolve. **ABI** é o contrato **binário** entre coisas já compiladas: se ele quebra e você não
recompila tudo, o programa linka, roda e corrompe memória. Assunto do demo 5.

**Pimpl — *Pointer to IMPLementation*.** O idioma em que a classe pública guarda **um** ponteiro
para uma struct **declarada mas não definida** no cabeçalho; toda a implementação vive no
`.cpp`. Demo 5, com o código completo.

**Header-only.** Biblioteca inteiramente em cabeçalhos, sem `.cpp` nem `.a`/`.so` para linkar:
basta incluir. Barato de integrar, caro de compilar (cada TU paga o custo do demo 1.2), e a
fonte clássica da violação de ODR quando duas versões dela convivem na mesma árvore.

**PCH e unity build.** *Precompiled headers*: o compilador salva o estado depois de processar
um conjunto fixo de cabeçalhos e o recarrega em cada TU. *Unity build*: um `.cpp` gerado que
faz `#include` de todos os outros, virando **uma** TU gigante — cada cabeçalho é processado uma
vez só. Ambos **reduzem o tempo** sem mudar o modelo; por isso o README diz que atacam o
sintoma, não a causa.

**Ferramentas citadas adiante.** `-flto` (*link time optimization*): o compilador guarda uma
representação intermediária no `.o`, e o **linker** otimiza vendo todas as TUs juntas — é isso
que dá ao `-Wodr` material para **comparar** as definições de classe de TUs diferentes e avisar
quando divergem. `gold` e `lld` são linkers alternativos ao `ld` clássico (mais rápidos), e
oferecem `--detect-odr-violations`. **ASan** (*AddressSanitizer*) é a instrumentação
`-fsanitize=address`, que pega acesso fora dos limites, uso após liberação e *double free* — as
consequências típicas de uma ODR violada.

## Por que estudar isto?

Porque os bugs desta área não dão erro de compilação. Eles linkam, rodam, e fazem outra coisa.

Duas medições deste subprojeto:

| Experimento | Resultado |
|---|---|
| duas TUs incrementam `inline int` 10× cada | **20 / 20** — um objeto só |
| duas TUs incrementam `static int` 10× cada | **10 / 10** — são duas variáveis |

O segundo é o "bug do `static` em cabeçalho": compila, linka, funciona enquanto houver uma
TU, e quebra no dia em que alguém inclui o cabeçalho num segundo `.cpp`. O sintoma ("o cache
não persiste") não aponta para o cabeçalho.

## Estrutura de arquivos

```
src/linkage/
├── main.cpp                 ← ponto de entrada, cor dark_sea_green
├── demo_traducao.hpp        ← 1 · pipeline, custo do #include, guards, os erros
├── demo_odr.hpp             ← 2 · a ODR, medida com duas TUs reais
├── demo_linkage.hpp         ← 3 · externa / interna / nenhuma, os 4 `static`
├── demo_extern_c.hpp        ← 4 · name mangling, extern "C", callbacks
├── demo_abi.hpp             ← 5 · ABI, pimpl, inline namespaces
├── comum.hpp                ← o cabeçalho compartilhado (o experimento)
├── unidade_a.cpp            ← TU 1
├── unidade_b.cpp            ← TU 2 — idêntica a A
├── widget.hpp / widget.cpp  ← o pimpl, em TU separada
├── c_api.h / c_api.c        ← a API em C, compilada por `cc`
├── meson.build
└── README.md
```

```bash
make build && ./dist/bin/linkage
```

---

## Demo 1: A unidade de tradução

### O pipeline

```
main.cpp ─┐
a.hpp ────┼→ [pré-processador] → uma TU → [compilador] → main.o → [linker] → exe
b.hpp ────┘        g++ -E                    g++ -c                  ld
```

```bash
g++ -E arq.cpp                  # só o pré-processador
g++ -c arq.cpp                  # até o objeto
nm -C arq.o                     # os símbolos, desmanglados
nm -C --undefined-only arq.o    # o que ele PRECISA
```

### O que um `#include` custa — medido

Linhas depois do pré-processador, GCC 13.3 com `-std=c++20`:

| inclui | linhas |
|---|---|
| arquivo vazio | 6 |
| `<cstddef>` | 253 |
| `<vector>` | 26.258 |
| `<algorithm>` | 28.270 |
| `<string>` | 31.825 |
| `<iostream>` | 43.165 |
| `<ranges>` | 44.309 |

Cada TU do projeto paga esse custo **inteiro**, de novo. Um projeto com 100 `.cpp` que incluem
`<string>` compila `<string>` cem vezes. É por isso que build de C++ é lento.

> A primeira versão desta tabela era chutada. Cinco dos seis números estavam errados —
> `<cstddef>` por um fator de 4. Reproduza no seu toolchain:
> ```bash
> for h in cstddef vector string ranges; do
>   echo "#include <$h>" > /tmp/t.cpp
>   printf '%-12s %s\n' "$h" "$(g++ -std=c++20 -E /tmp/t.cpp | wc -l)"
> done
> ```

As saídas, em ordem de eficácia: **módulos** (C++20 — ver [`src/modules/`](../modules/)),
**declaração adiantada** (`class Widget;` em vez de `#include "widget.hpp"`, quando o cabeçalho
só usa `Widget*` ou `Widget&`) e **pimpl** (seção 5). *Precompiled headers* e *unity builds*
encurtam o tempo sem mudar o modelo — atacam o sintoma.

### Include guards vs `#pragma once`

Incluir o mesmo cabeçalho **duas vezes na mesma TU** é trivial de acontecer (`a.hpp` inclui
`base.hpp`, `b.hpp` também, e o seu `.cpp` inclui os dois). Sem proteção, o pré-processador cola
o conteúdo duas vezes e o compilador responde `error: redefinition of 'struct X'`.

As duas proteções:

```cpp
// forma 1 — include guard: padrão, funciona em qualquer compilador
#ifndef CPP_ACADEMY_C_API_H     // se a macro ainda não existe…
#define CPP_ACADEMY_C_API_H     // …crie-a, e siga

/* ... conteúdo do cabeçalho ... */

#endif  /* CPP_ACADEMY_C_API_H */
```

```cpp
// forma 2 — pragma: uma linha, no topo do arquivo
#pragma once
```

| forma | está no padrão? | pega o mesmo arquivo via link simbólico | risco |
|---|---|---|---|
| `#ifndef X_H` … `#endif` | **sim** | sim — a chave é o conteúdo, não o caminho | **colisão de macro**: dois cabeçalhos com o mesmo nome de guard e um deles some silenciosamente |
| `#pragma once` | **não** (é extensão) | depende do compilador identificar o arquivo | nenhum, na prática |

`#pragma once` não é padrão, mas GCC, Clang, MSVC, ICC e praticamente todo compilador vivo o
suportam há décadas. **A política deste repositório:** `#pragma once` em todos os `.hpp`; e
**include guard** em [`c_api.h`](c_api.h), porque esse arquivo também é compilado como C e
segue a convenção de lá.

> ⚠ **Guards protegem contra inclusão dupla na MESMA TU. Eles não têm nada a ver com a ODR.**
> Cada TU do projeto ainda inclui o cabeçalho uma vez, e o que acontece *entre* as TUs é o
> assunto inteiro da seção 2. Confundir os dois é o mal-entendido mais comum da área: nenhum
> guard impede que `static int x;` num cabeçalho vire N variáveis.

### ⚠ Onde cada erro clássico aparece

| mensagem | etapa | causa típica |
|---|---|---|
| `No such file or directory` | pré-processador | `-I` faltando |
| `'X' was not declared` | compilador | `#include` faltando |
| `invalid use of incomplete type` | compilador | só a forward declaration |
| `undefined reference to 'f()'` | **linker** | declarou, não definiu |
| `multiple definition of 'f()'` | **linker** | função não-`inline` em cabeçalho |
| `undefined reference to vtable for X` | **linker** | a *key function* não tem corpo — ↓ |

> Reconhecer a **etapa** já resolve metade do problema: erro do compilador → falta uma
> **declaração**; erro do linker → falta (ou sobra) uma **definição**.

O caso da vtable: o GCC a emite na TU que define a **primeira função virtual não-inline e
não-pura**. Se você declarou `virtual void f();` e nunca a definiu, a vtable não é emitida em
lugar nenhum.

---

## Demo 2: A ODR, medida

A regra tem três partes, e só a terceira surpreende:

1. em **uma** TU, cada entidade pode ser declarada muitas vezes e definida no máximo uma;
2. no **programa**, cada função ou variável não-`inline` pode ser definida no máximo uma vez;
3. classes, funções `inline` e templates **podem** ser definidos em várias TUs — mas as
   definições precisam ser **token a token idênticas**. Se não forem, é **UB** (*undefined
   behavior*: o padrão não define nada, e o compilador pode presumir que a situação não ocorre),
   e **o linker não é obrigado a avisar**. Ele não avisa porque não compara: fica com uma das
   cópias COMDAT, arbitrariamente, e descarta as outras.

### Os endereços, de duas TUs reais

```
inline int f()         MESMO endereço        → 1 no programa inteiro
static int f()         endereços DIFERENTES  → 1 por TU
namespace{} int f()    endereços DIFERENTES  → 1 por TU
inline int var         MESMO endereço        → 1 no programa inteiro
static int var         endereços DIFERENTES  → 1 por TU
const int k = 42       endereços DIFERENTES  → 1 por TU
inline constexpr int   MESMO endereço        → 1 no programa inteiro
template<T> T f(T)     MESMO endereço        → 1 no programa inteiro
```

- `inline` → o linker **funde** as cópias das TUs;
- `static` / namespace anônimo → linkage **interna**, cada TU tem a sua;
- `const` em escopo de namespace → é implicitamente `static` em C++ (**em C seria externa**);
- template → instanciado em cada TU, e as cópias são fundidas como se fossem `inline`.

### ⚠ O `static` em cabeçalho

```
| variável         | vista de A | vista de B |
| contador_inline  |         20 |         20 |   ← um objeto
| contador_static  |         10 |         10 |   ← DUAS variáveis
```

Antes do C++17 a saída era `extern` no cabeçalho + definição num `.cpp`. Desde o C++17,
**`inline` resolve no próprio cabeçalho**. Use `inline`.

### `inline` não é sobre inlining

A palavra engana desde 1998. Ela significa: *"esta entidade pode ser definida em várias TUs; o
linker deve escolher uma e descartar as outras"*. Não obriga o compilador a expandir a chamada
— essa decisão é do otimizador, que a toma sozinho mesmo em funções não marcadas.

| quero… | uso |
|---|---|
| definir função em cabeçalho | `inline` |
| definir variável em cabeçalho | `inline` (C++17) |
| **forçar** a expansão | `[[gnu::always_inline]]` |
| **impedir** a expansão | `[[gnu::noinline]]` |

São implicitamente `inline`: funções definidas dentro do corpo da classe, `constexpr`,
`consteval`, e membros `static constexpr` (C++17).

### ⚠ A violação que linka e roda

```cpp
struct Widget {
    int a;
#ifndef NDEBUG
    int contador_de_depuracao;   // ⚠ só em debug
#endif
};
// a.cpp SEM -DNDEBUG → sizeof 8
// b.cpp COM -DNDEBUG → sizeof 4
```

O linker **não compara definições de classe** — ele só vê nomes de símbolo. O programa linka,
roda, e escreve fora dos limites quando cruza a fronteira.

Outras formas da mesma armadilha: `-D` diferente entre alvos, `#pragma pack` numa TU e não na
outra, `-std` diferente, linkar Debug com Release da mesma biblioteca
(`_GLIBCXX_ASSERTIONS` muda o layout de containers), duas versões de uma dependência
header-only na mesma árvore.

Defesas, da mais eficaz para a menos:

| defesa | o que ela faz |
|---|---|
| **flags uniformes** | é exatamente para isso que serve um build system: as mesmas `-D`, o mesmo `-std`, o mesmo `NDEBUG` em **todos** os alvos. Ver [`src/meson/`](../meson/) |
| `-flto` **+** `-Wodr` | com *link time optimization*, o `.o` guarda a representação intermediária e o **linker** enxerga todas as TUs juntas; só então o `-Wodr` tem como **comparar** as definições de classe e avisar quando duas divergem |
| `--detect-odr-violations` | opção dos linkers `gold` e `lld` (alternativas ao `ld` clássico): compara os tamanhos das seções COMDAT com a mesma chave e reclama quando não batem |
| **ASan** | `-fsanitize=address` não pega a violação em si, mas pega a **consequência** típica dela: a escrita fora dos limites que acontece quando um lado acha a struct menor |
| **nunca `#if` em torno de membro de classe pública** | a regra de higiene que torna as outras quatro desnecessárias |

---

## Demo 3: Os três tipos de linkage

"Linkage" responde a uma pergunta só: **este nome pode se referir à mesma entidade que um nome
igual em outro lugar?**

| escopo de namespace | linkage |
|---|---|
| `int x;` / `void f(){}` / `inline void f(){}` | **externa** |
| `static int x;` / `static void f(){}` | interna |
| `const int x = 1;` | interna ← *surpresa* |
| `constexpr int x = 1;` | interna (implica `const`) |
| `inline constexpr int x = 1;` | **externa** |
| `namespace { int x; }` | interna |
| `extern const int x = 1;` | **externa** |
| `typedef` / `using` / enum sem nome | nenhuma — ver ⓘ |

| escopo de bloco | linkage |
|---|---|
| `int x;` (local comum) | nenhuma |
| `static int x;` (local estático) | nenhuma — *mas dura o programa todo* |
| `extern int x;` | **externa** |

> ⓘ **Por que um alias não tem linkage nenhuma.** Linkage é sobre **entidades** que existem no
> binário — funções e objetos, coisas que viram símbolo. Um `typedef` ou `using` não cria
> entidade nenhuma: é só outro nome para um tipo que já existe, resolvido inteiramente pelo
> compilador e apagado antes de chegar ao `.o`. Não há o que o linker pudesse casar entre TUs,
> logo a pergunta "este nome se refere à mesma coisa que aquele?" nem se coloca. O mesmo vale
> para um `enum` sem nome. (Isso não isenta o alias da ODR: se duas TUs derem ao mesmo `using`
> significados diferentes, os tipos usados nas assinaturas divergem e você cai na violação
> silenciosa de 2.4.)

Não confunda linkage com **escopo** (onde o nome é visível) nem com **tempo de vida**. Uma
variável `static` dentro de uma função tem linkage **nenhuma**, escopo de bloco e tempo de vida
do programa inteiro.

### `static`: uma palavra, quatro significados

1. escopo de namespace → **linkage interna**
2. membro de classe → **um por classe**, não por objeto (não está no `sizeof` do objeto)
3. método de classe → **sem `this`**, chamável sem objeto
4. local de função → **tempo de vida do programa**, linkage nenhuma, escopo do bloco

Quatro conceitos ortogonais na mesma palavra — herança do C. Para o significado (1), prefira
**namespace anônimo**: mesmo efeito, sem ambiguidade, e funciona também para **tipos**, o que
`static` não faz.

> ⚠ Namespace anônimo **nunca** num cabeçalho: cada TU ganha a sua cópia — o mesmo bug de 2.2,
> agora com tipos.

### ⚠ `static` local e threads

Desde o C++11 a inicialização de um `static` local é **garantidamente thread-safe**. Medido: 8
threads chamando ao mesmo tempo → **1 inicialização**. É isso que torna o *Meyers singleton*
correto sem mutex.

O custo: o compilador emite uma **guard variable** — uma variável escondida ao lado do objeto,
que registra "já inicializei?" — e uma checagem dela em **cada** chamada, via as funções de
runtime `__cxa_guard_acquire` / `__cxa_guard_release`. Na primeira chamada isso é um caminho
completo com bloqueio; a partir da segunda é só uma **leitura atômica com ordem `relaxed`** —
uma leitura que não pode ser rasgada nem sofrer corrida, mas que **não** impõe nenhuma ordem
sobre as outras operações à volta, e por isso é quase tão barata quanto uma leitura comum
(ver [`src/concurrency2/`](../concurrency2/) para o vocabulário de `memory_order`). Barato, mas
não zero: num laço quente, chame a função uma vez e guarde a referência fora do laço.

> ⚠ A garantia é sobre a **inicialização**, não sobre o uso. Se várias threads mutam o objeto
> depois, você ainda precisa de sincronização ([`src/concurrency2/`](../concurrency2/)).
>
> ⚠ Globais de TUs diferentes têm ordem de inicialização **indefinida** entre si — o *static
> initialization order fiasco*, que o `static` local justamente evita.

### `extern`

Diz "isto é uma **declaração**, não uma definição". Era o único jeito de ter uma global
compartilhada antes do C++17; hoje `inline int x = 3;` faz o mesmo em uma linha.

Ainda é necessário para `extern const int k;` (forçar linkage externa numa constante),
`extern "C"` (linkage de **linguagem** — coisa diferente) e `extern template` (suprimir a
instanciação numa TU para acelerar o build).

---

## Demo 4: `extern "C"` e o name mangling

Em C, o símbolo de `int soma(int,int)` é `soma`. Em C++ é `_Z4somaii` — porque o C++ precisa
distinguir sobrecargas, namespaces e templates, e o formato de objeto só tem strings.

Os símbolos **reais** deste binário:

```
int f(int)          → _ZN13demo_extern_c8exemplos1fEi
int f(double)       → _ZN13demo_extern_c8exemplos1fEd
int f(int, int)     → _ZN13demo_extern_c8exemplos1fEii
extern "C" f_c(int) → f_c                              ← nome CRU
```

Decodificando `_ZN13demo_extern_c8exemplos1fEii`:

```
_Z                prefixo de nome manglado
N ... E           nome aninhado (namespaces)
13demo_extern_c   comprimento + nome
8exemplos         comprimento + nome
1f                comprimento + nome da função
ii                os PARÂMETROS: int, int
```

O mangling codifica namespace, nome e **tipos dos parâmetros**. Para funções comuns, **não**
codifica o tipo de retorno — é por isso que dois `f(int)` que diferissem só no retorno gerariam
o **mesmo** símbolo.

> ⓘ **Duas ressalvas honestas.** Primeira: a causalidade é o contrário do que a frase acima
> sugere. Proibir sobrecarga pelo tipo de retorno é **regra da linguagem**, anterior e
> independente do Itanium ABI — ela existe porque a resolução de sobrecarga acontece a partir
> dos **argumentos**, e numa chamada como `f(1);` cujo resultado é descartado não haveria como
> escolher. O mangling não codificar o retorno é **consequência** dessa regra, não a causa dela.
> Segunda: o Itanium ABI **codifica sim** o tipo de retorno no mangling de **templates de
> função** — justamente porque duas instanciações podem diferir só nele (`template<class T> T
> g();` chamado como `g<int>()` e `g<double>()`).

```bash
nm -C arq.o                    # nm já desmangla
echo '_Z4somaii' | c++filt     # desmangla à mão
```

> ⚠ O esquema **não é padronizado**. GCC e Clang usam o Itanium ABI; o MSVC usa outro. É por
> isso que um `.lib` do MSVC não linka com o MinGW.

### O idioma do cabeçalho compartilhado

```c
#ifdef __cplusplus
extern "C" {
#endif
    int soma_c(int a, int b);
#ifdef __cplusplus
}
#endif
```

Sem ele, o C++ geraria uma chamada para `_Z6soma_cii`, o linker responderia
`undefined reference to 'soma_c(int, int)'` — enquanto `nm c_api.o` mostraria, ali, um
`T soma_c`. É o erro de integração C/C++ mais comum que existe.

### ⚠ Os limites

`extern "C"` **não** permite: sobrecarga, templates, funções membro não-estáticas.
**Permite**: usar C++ dentro do corpo (classes, exceções, STL).

> ⚠ **Exceções não atravessam.** Uma exceção que escapa de uma função `extern "C"` chamada a
> partir de C é UB. O motivo é concreto: lançar uma exceção dispara o **unwinding** — o
> desenrolar da pilha, o mecanismo que percorre os quadros de chamada de volta **rodando os
> destrutores** de cada objeto local até achar um `catch`. Ele não adivinha nada: consulta
> **tabelas** que o compilador emitiu junto com o código. Os quadros gerados pelo compilador
> **C** normalmente não têm essas tabelas, então o desenrolar não sabe o que fazer ali — os
> destrutores intermediários não rodam, e o programa costuma terminar em `std::terminate`.
> A fronteira precisa ser à prova:
>
> ```cpp
> extern "C" int minha_api(int x) noexcept {
>     try { return impl(x); }
>     catch (const std::exception& e) { guarda_erro(e.what()); return -1; }
>     catch (...) { return -2; }
> }
> ```
>
> Erros viram **códigos de retorno** na fronteira — é o motivo de toda API de C ser assim.
>
> ⚠ Um objeto alocado com `new` no C++ **não** pode ser liberado com `free` no C. Exporte
> sempre um `minha_api_free(ptr)` junto do que aloca.

### Callbacks: o C chamando de volta o seu C++

Uma API de C recebe um **ponteiro de função** cru. É o que `c_api.h` faz:

```c
/* c_api.h — lado C */
typedef int (*transformador_t)(int);
int aplica_c(transformador_t f, int v);
```

Uma **lambda sem captura** converte para ponteiro de função e funciona na prática. Uma **lambda
com captura não converte** — um ponteiro de função tem 8 bytes e não há onde guardar o estado
capturado.

O idioma que resolve isso é o parâmetro `void* user_data` que quase toda API de C séria
oferece, mais uma função `extern "C"` de **trampolim**: uma ponte que recebe o `void*`, o
converte de volta para o tipo real e chama o objeto invocável de verdade.

```c
/* lado C: a API leva o callback E um ponteiro opaco que ela devolve intacto */
void percorre(void (*cb)(int valor, void* user), void* user);
```

```cpp
// lado C++
#include <functional>

// O trampolim: `extern "C"` para ter linkage C, e livre (sem captura, sem `this`).
extern "C" void trampolim(int valor, void* user) {
    // `user` é o MESMO ponteiro que passamos lá embaixo — só perdeu o tipo no caminho.
    (*static_cast<std::function<void(int)>*>(user))(valor);
}

void uso() {
    int soma = 0;
    std::function<void(int)> acumula = [&soma](int v) { soma += v; };   // COM captura

    percorre(&trampolim, &acumula);   // o estado viaja pelo void*, não pelo ponteiro de função
    //                     ▲
    //                     `acumula` precisa continuar viva durante a chamada inteira
}
```

> ⚠ O `void*` **não é dono de nada**: quem garante que `acumula` sobrevive à chamada é você.
> Com uma API que guarda o callback para chamar depois (e não durante), o objeto precisa viver
> até o cancelamento — e aí a API de C também precisa oferecer um jeito de cancelar.
>
> ⚠ O trampolim precisa ser `noexcept` na prática, ou embrulhar tudo num `try`: uma exceção que
> escapasse dele atravessaria os quadros de C, e cai na regra da seção anterior.

---

## Demo 5: ABI — o contrato binário

**API** é o contrato de código-fonte: recompilar resolve qualquer quebra.
**ABI** é o contrato binário: quebrar a ABI sem quebrar a API é o pior dos mundos — recompila,
linka, e corrompe memória em runtime.

Fazem parte da ABI: os nomes manglados, o `sizeof` e o **layout** de cada classe exposta, a
**ordem das entradas na vtable**, a convenção de chamada, o formato das exceções — e o layout
de **todo tipo da std que apareça na interface**.

Este último é o que pega: se a sua biblioteca tem `void configura(const std::string&)`, então o
layout de `std::string` faz parte da **sua** ABI. Foi exatamente isso o rompimento do GCC 5
(`_GLIBCXX_USE_CXX11_ABI`): `std::string` deixou de ser **copy-on-write** (*cópia na escrita*:
a implementação antiga fazia as cópias de uma string **compartilharem o mesmo buffer** e só
duplicavam de verdade na primeira escrita — economia de memória que virou incompatível com o
modelo de memória do C++11, porque exige um contador de referências correto sob concorrência e
faz uma leitura aparentemente inocente poder alocar). O layout mudou, e o símbolo passou a
carregar `__cxx11` no nome — para que a incompatibilidade virasse **erro de linker** em vez de
corrupção silenciosa. Foi uma decisão deliberada: melhor não linkar do que linkar errado.

### ⚠ Quebra a ABI, não quebra a API

| mudança | API | ABI |
|---|---|---|
| acrescentar / reordenar membro de dado | ok | ✗ |
| acrescentar / reordenar método **virtual** | ok | ✗ |
| tornar virtual um método comum | ok | ✗ |
| acrescentar parâmetro com valor padrão | ok | ✗ |
| tornar `inline` uma função | ok | ✗ |
| acrescentar método **não** virtual | ok | ok |
| acrescentar sobrecarga nova | ok | ok |
| mudar o **corpo** de uma função | ok | ok |

"Acrescentar parâmetro com default" surpreende: o valor padrão é gravado no lado do
**chamador**. Um cliente antigo, não recompilado, chama um símbolo que não existe mais.

"Acrescentar método virtual" merece o mecanismo por extenso, porque sem ele a linha é uma
afirmação, e com ele é uma dedução inevitável:

1. cada classe polimórfica tem **um** array de ponteiros de função no binário — a **vtable**,
   uma por classe, medida em [`src/layout/`](../layout/);
2. a **posição** de cada método nesse array (o *slot*) é decidida **em compilação**, e é
   gravada **no chamador**: `p->f()` vira, na prática, `p->vptr[2](p)` com o índice fixo;
3. acrescentar um método virtual **no meio** da classe empurra todos os índices seguintes uma
   casa para baixo;
4. o cliente antigo, que não foi recompilado, continua chamando `vptr[2]` — que agora aponta
   para **outra função**. Ele executa o corpo errado, com os argumentos errados, sem nenhum
   aviso. Se as assinaturas diferirem, os registradores são interpretados como outra coisa.

Por isso a regra de quem publica biblioteca binária é acrescentar métodos virtuais **só no
fim** da classe — e mesmo assim com cuidado, porque um derivado do cliente pode ter os seus
próprios slots depois.

### Pimpl — o firewall de ABI

**Pimpl = *Pointer to IMPLementation*.** A classe pública guarda **um** ponteiro para uma struct
**declarada mas não definida** no cabeçalho; toda a implementação vive no `.cpp`. É o código de
[`widget.hpp`](widget.hpp) e [`widget.cpp`](widget.cpp), inteiro:

```cpp
// widget.hpp — o que o cliente vê
#pragma once
#include <memory>
#include <string>

namespace demo_abi {

class Widget {
    struct Impl;                    // ← só DECLARADA. O cliente nunca sabe o que tem dentro.
    std::unique_ptr<Impl> pimpl_;   // ← o único membro de dado: 8 bytes, para sempre

public:
    explicit Widget(std::string nome);

    // Declarados aqui, DEFINIDOS no .cpp — ver o ⚠ abaixo.
    ~Widget();
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;

    std::string descricao() const;
    void        renomear(std::string novo);
};

}  // namespace demo_abi
```

```cpp
// widget.cpp — a metade escondida
#include "widget.hpp"

namespace demo_abi {

// A definição que o cabeçalho nunca vê. Acrescentar membros AQUI não muda
// sizeof(Widget) e portanto NÃO quebra a ABI.
struct Widget::Impl {
    std::string nome;
    int         acessos = 0;
};

Widget::Widget(std::string nome)
    : pimpl_{std::make_unique<Impl>(Impl{std::move(nome), 0})} {}

// `= default` AQUI, onde `Impl` é um tipo completo:
Widget::~Widget()                            = default;
Widget::Widget(Widget&&) noexcept            = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

std::string Widget::descricao() const {
    ++pimpl_->acessos;
    return pimpl_->nome + " (acessos: " + std::to_string(pimpl_->acessos) + ")";
}

}  // namespace demo_abi
```

Repare que o cabeçalho **não inclui** `<vector>`, nem a biblioteca de JSON, nem nada que a
`Impl` use — quem inclui é o `.cpp`. Esse é o ganho de tempo de compilação; o ganho de ABI vem
de graça junto.

`sizeof(Widget)` = **8 bytes** (um `unique_ptr`), e continuará 8 para sempre, independentemente
do que a `Impl` ganhe ou perca.

**Ganhos:** ABI estável e build rápido (o cabeçalho não inclui nada do que a `Impl` usa).
**Custos:** uma alocação por objeto, uma indireção por acesso, e nada inlina através dela.

> ⚠ `~Widget()` **não** pode ser `= default` no cabeçalho: `unique_ptr<Impl>` precisa do tipo
> **completo** para destruir. O erro é críptico —
> `static assertion failed: can't delete an incomplete type`. Declare no `.hpp`, `= default` no
> `.cpp`, depois da definição de `Impl`. O mesmo vale para o move.
>
> ⚠ A segunda armadilha custou um erro de linker na primeira versão deste subprojeto:
> `Widget` estava declarada em `demo_abi.hpp`, e `widget.cpp` incluía esse cabeçalho — o que
> fez as funções `demo_*()`, que não são `inline`, serem definidas em **duas TUs**:
> `multiple definition of 'demo_abi::demo_o_que_e()'`. É o erro do demo 1.4, ao vivo. A
> correção foi separar `widget.hpp`: um cabeçalho com só o que precisa ser compartilhado, e
> nada definido.

### Inline namespaces

```cpp
namespace biblioteca {
    inline namespace v2 { const char* versao(); }   // biblioteca::versao()
    namespace        v1 { const char* versao(); }   // biblioteca::v1::versao()
}
```

O usuário escreve `biblioteca::versao()` sem versão, mas o símbolo carrega o `v2` no mangling.
Duas versões incompatíveis **coexistem** no mesmo processo. É como a libstdc++ faz —
`std::__cxx11::string` é exatamente isto. Também é o mecanismo de
`std::literals::chrono_literals`.

### Por que a std não conserta o que está ruim

`std::unordered_map` (encadeamento por listas, um nó alocado por elemento), `std::regex` (ordens
de grandeza mais lento que a **RE2**, a engine de expressões regulares do Google, que garante
tempo linear — medido em [`src/strings/`](../strings/)), `std::vector<bool>`.
Consertar qualquer um muda o layout → quebra a ABI → toda biblioteca binária do planeta que
exponha um desses na interface para de funcionar.

> **A lição para o seu código:** se você publica uma biblioteca binária, decida **cedo** o que
> é ABI. Depois de publicado, a escolha é irreversível — pergunte à std.

---

## Como validar

**1. O caminho normal — o build do repositório.**

```bash
make build             # configura (conan + meson), compila e instala em ./dist
./dist/bin/linkage     # roda
```

`make build` supõe que `make configure` já rodou uma vez (é ele que cria `build/` e o
`conanbuild.sh` sem o qual o meson não acha o `fmt`). Na primeira vez, `make all`.

O que se espera ver: cinco blocos numerados em verde-mar, do `1 · A unidade de tradução` ao
`5 · ABI — o contrato binário`. Os números que **não** podem falhar:

- em `2.1`, `inline`, `inline constexpr` e o template com o **mesmo** endereço nas duas TUs; e
  `static`, o namespace anônimo e `const int k` com endereços **diferentes**;
- em `2.2`, `contador_inline` em **20/20** e `contador_static` em **10/10**;
- em `3.4`, `8 threads → 1 inicialização`;
- em `4.2`, `soma_c(20, 22) = 42`, vindo de um `.c` compilado pelo `cc`.

**2. Compilar à mão as cinco TUs, com todos os avisos.** Repare que são **dois compiladores**:

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)

cc  -std=c11 -Wall -Wextra -Wpedantic -c -Isrc/linkage \
    src/linkage/c_api.c -o /tmp/capi.o

g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -I$FMTP/include -Isrc/linkage \
    src/linkage/main.cpp src/linkage/unidade_a.cpp src/linkage/unidade_b.cpp \
    src/linkage/widget.cpp /tmp/capi.o -o /tmp/lk -L$FMTP/lib -lfmtd -pthread

LD_LIBRARY_PATH=$FMTP/lib /tmp/lk | head -30
```

Esperado: **nenhuma linha de aviso** em nenhum dos dois, e o demo 1 na tela.

**3. Sob os sanitizers** — inclusive o `.c`, que precisa ser instrumentado junto:

```bash
cc  -std=c11 -O1 -g -fsanitize=address,undefined -c -Isrc/linkage \
    src/linkage/c_api.c -o /tmp/capisan.o

g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include -Isrc/linkage \
    src/linkage/main.cpp src/linkage/unidade_a.cpp src/linkage/unidade_b.cpp \
    src/linkage/widget.cpp /tmp/capisan.o -o /tmp/lksan -L$FMTP/lib -lfmtd -pthread

LD_LIBRARY_PATH=$FMTP/lib /tmp/lksan > /dev/null
echo "código de saída: $?"
```

Esperado: código de saída **0** e **nenhuma** linha `runtime error:` ou
`ERROR: AddressSanitizer` em `stderr`.

**Estado atual (verificado):** sem warnings com `-Wall -Wextra -Wpedantic` nos dois
compiladores em `-O2`, e **0 achados sob ASan+UBSan**.

**4. Ver os símbolos com os próprios olhos** — é o que fecha o assunto:

```bash
nm -C ./dist/bin/linkage | grep -i 'demo_extern_c.*exemplos::f'   # as três sobrecargas
nm    ./dist/bin/linkage | grep ' T soma_c'                       # o nome CRU, vindo do C
echo '_ZN13demo_extern_c8exemplos1fEii' | c++filt                 # desmangla à mão
```

### Sobre o `#pragma GCC diagnostic` de `comum.hpp`

Um `#pragma GCC diagnostic` em [`comum.hpp`](comum.hpp) suprime
`-Wunused-function`/`-Wunused-variable`: as entidades de linkage **interna** dali são usadas por
`unidade_a.cpp` e `unidade_b.cpp`, mas **não** por `main.cpp` — que também inclui o cabeçalho.
**Esse aviso é a própria lição**: cada TU ganha a sua cópia, use-a ou não. Suprimimos porque a
duplicação é o objeto de estudo; em código de produção o aviso deve ser **corrigido**, não
calado.

## Referências

### Neste repositório

- [`src/layout/`](../layout/) — **o complemento direto deste subprojeto.** O `sizeof` e o
  layout de classe que a seção 5 chama de "parte da ABI" são medidos lá, vptr e vtable
  inclusive.
- [`src/modules/`](../modules/) — a resposta do C++20 ao custo do `#include` (demo 1.2).
- [`src/meson/`](../meson/) — o build system e as flags uniformes que evitam a ODR silenciosa.
- [`src/undefined_behavior/`](../undefined_behavior/) — o que é UB e os sanitizers que pegam as
  consequências de uma ODR violada.
- [`src/design-of-pattern/singleton/`](../design-of-pattern/singleton/) — o Meyers singleton, que
  é o `static` local da seção 3.4 aplicado.
- [`src/concurrency2/`](../concurrency2/) — a sincronização que a garantia de inicialização
  thread-safe do `static` local **não** dá.
- [`src/errors/`](../errors/) — `error_code`, e por que toda fronteira de C usa códigos de
  retorno em vez de exceções.
- [`src/strings/`](../strings/) — a medição de `std::regex` contra a RE2 citada na seção 5.
- Os fontes deste subprojeto, que valem tanto quanto o texto:
  [`comum.hpp`](comum.hpp) (o experimento), [`widget.hpp`](widget.hpp) /
  [`widget.cpp`](widget.cpp) (o pimpl) e [`c_api.h`](c_api.h) / [`c_api.c`](c_api.c) (a
  fronteira com C).

### Fora daqui

- *Itanium C++ ABI* (itanium-cxx-abi.github.io/cxx-abi/abi.html) — a especificação de mangling,
  layout e vtable que GCC e Clang seguem. A seção 5.1 é o mangling completo.
- KDE Community Wiki, *Policies/Binary Compatibility Issues With C++* — a lista canônica do que
  quebra e do que não quebra a ABI. É dela que a tabela da seção 5 descende.
- Ulrich Drepper, *How To Write Shared Libraries* — o texto de referência sobre símbolos,
  visibilidade e o custo de cada um.
- `nm(1)`, `c++filt(1)`, `objdump(1)`, `readelf(1)`, `ldd(1)` — as cinco ferramentas que
  transformam este assunto de teoria em observação. Comece por `nm -C`.
