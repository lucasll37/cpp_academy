# modules — Módulos do C++20

## O que é este projeto?

O `#include` do C++ é **cópia de texto**: o pré-processador abre o arquivo citado e cola o
conteúdo dele dentro do seu `.cpp`, literalmente, antes de o compilador ver qualquer coisa.
Funciona desde 1972 (o C herdou isso do assembler), e **todos** os problemas de compilação de
C++ nascem daí.

Os **módulos** do C++20 substituem essa colagem por uma coisa diferente: uma unidade compilada
**uma vez** e lida em **forma binária** por quem a usa. Este subprojeto mostra um módulo real
funcionando no GCC 13, o que ele isola, o que ele custa e por que a adoção demorou cinco anos.

```bash
cd src/modules
make run
```

Saída esperada ao final: `✓ Todos os demos de modulos concluidos!`

> ⚠ **Este subprojeto não faz parte do build do Meson da raiz.** Ele tem um `Makefile`
> próprio, e por isso **não** aparece em `dist/bin/`. A razão está em
> [Por que fora do Meson](#por-que-fora-do-meson) — e é conteúdo didático, não preguiça.
> É a mesma escolha de [`../cmake/`](../cmake/), que usa CMake próprio.

---

## Antes de começar

Este README usa sete termos o tempo todo. Aqui está cada um, em uma frase, **antes** de
qualquer código. Se você já os conhece, pule para [Por que estudar isto?](#por-que-estudar-isto).

**Pré-processador.** O primeiro programa que toca o seu código. Ele não entende C++: só
manipula texto. `#include <x>` → cola o arquivo `x` ali. `#define N 10` → troca todo `N` por
`10` daqui para a frente. `#ifdef` → apaga trechos. Só depois disso o compilador entra.

**TU — unidade de tradução** (*translation unit*). Um `.cpp` **depois** que o pré-processador
colou todos os `#include` dele. É a unidade que o compilador realmente vê, e é enorme: um
`#include <string>` sozinho vira **31 828 linhas** nesta máquina (medido logo abaixo). Cada TU é compilada **do zero**, em
isolamento, e produz um `.o`.

```bash
# veja uma TU com os próprios olhos: o -E para no pré-processador
printf '#include <string>\nint main(){}\n' > /tmp/tu.cpp
g++ -std=c++20 -E /tmp/tu.cpp | wc -l      # nesta máquina: 31828
```

**Linkagem (*link*).** O passo final: juntar os vários `.o` num executável, resolvendo cada
nome usado numa TU contra a definição que está em outra.

**Linkage interna × externa.** Propriedade de um nome, que decide se ele **atravessa** a
fronteira da TU. Linkage **externa** = existe **um** só no programa inteiro, e outras TUs
podem alcançá-lo. Linkage **interna** = o nome só vale dentro da própria TU, e cada TU que
o declara ganha **uma cópia distinta** dele. `static` no escopo de namespace dá linkage
interna; `const` e `constexpr` no escopo de namespace **também** — e é justamente essa regra
pouco lembrada que produz a armadilha central deste subprojeto.

**ODR — One Definition Rule.** "Uma definição, e só uma, por programa." Violar isso é
**comportamento indefinido não diagnosticado**: o compilador não é obrigado a avisar, o linker
costuma escolher uma definição em silêncio, e o programa faz outra coisa. Duas TUs que
incluíram versões diferentes do mesmo header é o caso clássico.
Aprofundamento: [`../linkage/`](../linkage/).

**Global module fragment (GMF).** A região entre `module;` e `export module X;` no topo de uma
interface de módulo. É a **única** parte de um módulo onde o pré-processador ainda age — e é
por isso que ela existe: `#include` é colagem de texto, e o corpo de um módulo **não** é texto
colável. Um `#include` fora do GMF é erro de compilação.

**`.gcm`.** *GCC Compiled Module*: o artefato binário que o GCC escreve em `gcm.cache/` ao
compilar uma interface. Contém a interface já analisada — inclusive a **árvore sintática** dos
templates, que é o motivo de templates funcionarem em módulos. O `.gcm` **não entra no link**;
ele é consumido em tempo de compilação, pelo próximo `g++`.

**`-fmodules-ts`.** A flag do GCC que liga módulos. O `ts` é resquício do *Technical
Specification* que precedeu o padrão; no GCC 14+ o nome canônico passou a ser `-fmodules`, e
`-fmodules-ts` continua aceito como sinônimo. Aqui usamos `-fmodules-ts` porque o compilador
desta máquina é o **GCC 13.3.0**.

---

## Por que estudar isto?

Porque o `#include` é uma cópia de texto, e os cinco problemas abaixo são consequências diretas
disso — não são acidentes de implementação:

| Problema | Causa (sempre a mesma) |
|---|---|
| compilação lenta | o mesmo header é reprocessado **em cada TU**; 100 `.cpp` que incluem `<string>` analisam `<string>` 100 vezes |
| macros vazando | `#define` do incluidor afeta o incluído, e vice-versa — porque tudo é o mesmo texto |
| ordem de include importando | o header B só compila se A vier antes, porque B conta com o texto de A já colado |
| include guards | necessários porque, sendo colagem, incluir duas vezes cola duas vezes |
| tudo é público | não há como esconder o que o header incluiu: o texto dele já está no seu arquivo |

Módulos resolvem os cinco de uma vez, com um único mecanismo — a interface vira um artefato
binário, não texto. O preço é uma **ordem de compilação obrigatória**, e é um preço alto: é
exatamente ele que atrasou a adoção por cinco anos.

---

## Estrutura de arquivos

```
src/modules/
├── geometria.cppm     ← interface de módulo, sem dependências (Demo 1)
├── formatacao.cppm    ← importa e REEXPORTA geometria        (Demo 2)
├── main.cpp           ← o consumidor                         (Demos 3 e 4)
├── Makefile           ← o build, com a ordem obrigatória explícita
├── .gitignore         ← ignora os artefatos gerados abaixo
└── README.md
```

Gerados pelo `make`, e ignorados pelo Git:

```
├── geometria.o  formatacao.o  main.o    ← código-objeto
├── gcm.cache/geometria.gcm              ← 3,3 KB — a interface binária
├── gcm.cache/formatacao.gcm             ← 2,1 MB — inclui <string> transitivamente
└── modules_demo                         ← o executável
```

Arquivos: [`geometria.cppm`](geometria.cppm) · [`formatacao.cppm`](formatacao.cppm) ·
[`main.cpp`](main.cpp) · [`Makefile`](Makefile)

---

## A anatomia de uma interface de módulo

Uma interface de módulo tem quatro regiões, nesta ordem. A extensão `.cppm` é **convenção**
(o Clang a reconhece; o GCC exige `-x c++`); o que define o arquivo como interface é a
declaração `export module NOME;`.

```cpp
module;                        // ← 1. abre o global module fragment
#include <cmath>               //      a ÚNICA região onde o pré-processador ainda age

export module geometria;       // ← 2. a declaração que define o módulo
                               //      daqui para baixo, nada é "texto colável"

namespace detalhe {            // ← 3. o que NÃO tem export: invisível para quem importa
    double quadrado(double x) { return x * x; }
}

export namespace geometria {   // ← 4. a interface pública
    struct Ponto { double x{}, y{}; };
    double distancia(const Ponto&, const Ponto&);

    template <typename T> T maior(T a, T b);   // templates funcionam

    inline constexpr double PI = 3.14159265358979323846;   // o `inline` é obrigatório
}
```

**Por que o GMF existe.** Um header antigo (`<cmath>`) é texto que precisa ser **colado**. O
corpo de um módulo não é uma sequência de texto pré-processável — é uma unidade analisada e
serializada. As duas coisas não convivem, então o padrão reservou uma faixa no topo,
antes de `export module`, onde o mundo antigo ainda vale. Fora dessa faixa, `#include` é erro.

**O que atravessa o `import`, e o que não atravessa:**

| | atravessa? |
|---|---|
| nomes marcados com `export` | **sim** |
| nomes sem `export` (ex.: `detalhe::quadrado`) | não |
| nomes vindos do `#include` no GMF (ex.: `std::sqrt`) | não |
| macros (`#define`) definidas no módulo | **não** — em nenhum sentido |
| macros definidas por quem importa | não afetam o módulo |

Essa última linha é a mais valiosa e a menos óbvia: um `#define max(a,b)` no seu `.cpp` **não**
consegue mais estragar a biblioteca que você importou.

---

## Os demos, um a um

Os quatro blocos que [`main.cpp`](main.cpp) imprime, na ordem em que aparecem na tela.

### Demo 1 · Usando um módulo importado

```cpp
import formatacao;             // e, por `export import`, também `geometria`

const geometria::Ponto a{0.0, 0.0};
const geometria::Ponto b{3.0, 4.0};
geometria::distancia(a, b);    // 5.00
geometria::area_circulo(2.0);  // 12.5664
geometria::PI;                 // 3.1415926536
geometria::maior(3, 7);        // 7   ← template, instanciado a partir do .gcm
geometria::maior(2.5, 1.5);    // 2.5
```

O ponto: **templates funcionam**. Isso surpreende quem sabe que um template precisa da
definição visível para ser instanciado. Num header, a definição estava visível porque o texto
tinha sido colado. Num módulo, o `.gcm` guarda a **árvore sintática** já analisada — o
compilador instancia a partir dela, sem reprocessar texto nenhum.

### Demo 2 · `export import` — reexportação

```cpp
// formatacao.cppm
export import geometria;   // importa E reexporta
```

`main.cpp` faz apenas `import formatacao;` e enxerga `geometria` também. Sem o `export`,
`formatacao` usaria `geometria` internamente e o consumidor **não** a veria.

É a versão explícita e controlada do que headers fazem por acidente: um header que inclui
outro reexporta tudo, queira você ou não.

⚠ **É aqui que a ordem de compilação nasce.** `formatacao.cppm` só compila se
`gcm.cache/geometria.gcm` já existir — e ele só existe depois que `geometria.cppm` compilou.

### Demo 3 · O que NÃO vaza

[`geometria.cppm`](geometria.cppm) faz `#include <cmath>` no GMF e usa `std::sqrt` internamente.
O [`main.cpp`](main.cpp) **não** inclui `<cmath>`. Descomente a linha que já está lá:

```cpp
// double r = std::sqrt(16.0);   // ← descomente
```

```
error: 'sqrt' is not a member of 'std'
```

Com um header, `std::sqrt` estaria disponível sem você ter pedido — e a poluição é
**cumulativa**: um header que inclui outro que inclui outro traz milhares de nomes.

O mesmo vale para `detalhe::quadrado`, definida sem `export`:

```cpp
// detalhe::quadrado(2.0);   // → error: 'detalhe' has not been declared
```

> **Nuance que quase todo tutorial omite:** "não exportado" **não** é a mesma coisa que
> "inalcançável". Uma entidade não exportada continua podendo ser alcançada indiretamente —
> por *ADL* (busca de função no namespace dos argumentos) a partir de um tipo que **é**
> exportado, ou por instanciação de um template exportado que a use internamente. `export`
> controla o que é **nomeável** pelo consumidor, não o que é fisicamente inatingível. Se você
> quer garantia de inacessibilidade, o instrumento é o `module :private;` — que o GCC 13
> ainda não implementa (veja a seção seguinte).

### Demo 4 · O que isso custa e o que economiza

| | Header | Módulo |
|---|---|---|
| **compilação** | reprocessado em cada TU | compilado **uma** vez; consumidores leem o `.gcm` |
| **ordem** | irrelevante — cada TU é independente | **obrigatória** — interface antes do consumidor |
| **isolamento** | tudo vaza | só o que tem `export` |
| **macros** | atravessam nos dois sentidos | **não** atravessam |
| **include guards** | necessários | desnecessários |
| **templates** | precisam estar no header | ficam no `.gcm` (árvore sintática) |
| **ferramental** | trivial: `-MMD` resolve | exige um **scanner de dependências** |

Aquela última linha é a fatura. As outras seis são o ganho.

---

## Um módulo com mais de um arquivo

Os dois `.cppm` desta pasta são interfaces de arquivo único — bom para aprender, insuficiente
para um projeto real. As três construções abaixo são o que falta, e **todas foram compiladas e
executadas nesta máquina** (GCC 13.3.0) enquanto este README era escrito.

### 1. Unidade de implementação — separar declaração de definição

A interface declara; um segundo arquivo define. O arquivo de implementação começa com
`module NOME;` **sem** `export`:

```cpp
// forma.cppm — a INTERFACE (só declara)
export module forma;
export namespace forma { double area(double r); }
```

```cpp
// forma.cpp — a IMPLEMENTAÇÃO (mesmo módulo, sem `export`)
module forma;
inline constexpr double PI = 3.14159265358979323846;   // privado a esta unidade
namespace forma { double area(double r) { return PI * r * r; } }
```

```bash
g++ -std=c++20 -fmodules-ts -x c++ -c forma.cppm -o forma_iface.o   # produz o .gcm
g++ -std=c++20 -fmodules-ts        -c forma.cpp  -o forma_impl.o    # consome o .gcm
g++ -std=c++20 -fmodules-ts        -c m.cpp      -o m.o
g++ -std=c++20 -fmodules-ts forma_iface.o forma_impl.o m.o -o prog
./prog        # → 12.5664
```

O que isso compra: mudar o **corpo** de `area()` recompila só `forma.cpp`. Quem importa
`forma` não é tocado, porque a interface não mudou. É o benefício de compilação incremental
que headers nunca deram (mexer no corpo de uma função `inline` num header recompila o mundo).

### 2. Partições — quebrar um módulo grande sem quebrar a interface

Uma partição é um pedaço interno de um módulo. De fora, o módulo continua sendo **um só**:

```cpp
// geo-formas.cppm — a partição
export module geo:formas;
export namespace geo { struct Ponto { double x{}, y{}; }; }
```

```cpp
// geo.cppm — a interface PRIMÁRIA
export module geo;
export import :formas;      // agrega a partição à interface pública
export namespace geo { double soma(const Ponto& p) { return p.x + p.y; } }
```

```cpp
// m.cpp — o consumidor não sabe que existem partições
import geo;
int main() { geo::Ponto p{1.5, 2.5}; return (int)geo::soma(p); }   // → 4
```

```bash
g++ -std=c++20 -fmodules-ts -x c++ -c geo-formas.cppm -o geo-formas.o
g++ -std=c++20 -fmodules-ts -x c++ -c geo.cppm        -o geo.o
g++ -std=c++20 -fmodules-ts        -c m.cpp           -o m.o
g++ -std=c++20 -fmodules-ts geo-formas.o geo.o m.o -o prog
```

O GCC nomeia o `.gcm` da partição trocando o `:` por `-`:

```
gcm.cache/geo-formas.gcm      ← a partição geo:formas
gcm.cache/geo.gcm             ← a interface primária
```

Duas notas:

- `import :formas;` (sem `export`) usaria a partição **internamente**, sem revelá-la;
  `export import :formas;` a agrega à interface pública. É a mesma distinção do Demo 2.
- A sintaxe `import :parte;` só é válida **dentro** do próprio módulo. De fora não existe
  jeito de importar uma partição: é isso que a torna um detalhe de implementação de verdade.

### 3. `module :private;` — o que nem as outras unidades veem

O *private module fragment* é a região no fim de uma interface cujo conteúdo não é visível
para ninguém, e cuja alteração **não** invalida os consumidores:

```cpp
export module cx;
export namespace cx { int dobro(int x); }

module :private;                                   // ← daqui para baixo, ninguém vê
namespace cx { int dobro(int x) { return 2 * x; } }
```

⚠ **O GCC 13.3 não implementa isso.** Medido:

```
cx.cppm:3:1: sorry, unimplemented: private module fragment
    3 | module :private;
      | ^~~~~~
```

Use uma unidade de implementação (item 1) enquanto isso.

---

## Os comandos g++, em ordem

A ordem é a tese central deste subprojeto, então aqui está ela como comando, e não só como
regra do [`Makefile`](Makefile). É exatamente o que o `make` executa:

```bash
CXXFLAGS="-std=c++20 -fmodules-ts -Wall -Wextra -Wpedantic"

# 1. geometria — não depende de ninguém
g++ $CXXFLAGS -x c++ -c geometria.cppm  -o geometria.o
#    produz:  geometria.o  +  gcm.cache/geometria.gcm

# 2. formatacao — faz `export import geometria`, então PRECISA do .gcm acima
g++ $CXXFLAGS -x c++ -c formatacao.cppm -o formatacao.o
#    produz:  formatacao.o +  gcm.cache/formatacao.gcm

# 3. main — faz `import formatacao`, então precisa dos dois .gcm
g++ $CXXFLAGS        -c main.cpp        -o main.o
#    produz:  main.o        (nenhum .gcm: main.cpp não é interface de módulo)

# 4. link — só os .o entram; nenhum .gcm aparece aqui
g++ $CXXFLAGS geometria.o formatacao.o main.o -o modules_demo
```

Inverta os passos 1 e 2 e o passo 2 falha, porque `gcm.cache/geometria.gcm` ainda não existe.
Com headers, qualquer ordem funcionaria.

---

## ⚠ Armadilhas

### 1. `constexpr` no escopo de namespace não é exportável sem `inline`

Esta é a armadilha que mais custa tempo, e o motivo é a definição de **linkage interna** lá do
começo deste README: uma variável `constexpr` em escopo de namespace (global ou nomeado) tem
**linkage interna** por padrão, exatamente como um `const`. Um nome de linkage interna não
atravessa a fronteira da unidade — e `export` não muda isso. O resultado é que a variável
*parece* exportada e não está.

```cpp
export namespace geometria {
    constexpr double PI = 3.14;          // ✗ 'PI' is not a member of 'geometria'
    inline constexpr double PI = 3.14;   // ✓
}
```

O `inline` dá linkage externa **e** permite a definição repetida em várias unidades sem violar
a ODR. Sem ele, cada unidade teria a sua cópia — que é o que "interna" quer dizer.

**A matriz completa, medida no GCC 13.3.0** (cada linha foi compilada e executada; ✓ = o
consumidor enxerga o nome):

| declaração na interface do módulo | com GMF | sem GMF |
|---|:---:|:---:|
| `export constexpr double PI = 3.14;` (escopo global) | ✗ | ✗ |
| `export inline constexpr double PI = 3.14;` (escopo global) | ✓ | ✓ |
| `export namespace n { constexpr double PI = 3.14; }` | ✗ | ✗ |
| `export namespace n { inline constexpr double PI = 3.14; }` | ✓ | ✓ |
| `export int contador = 7;` (linkage externa por natureza) | ✓ | ✓ |

Os erros exatos, conforme o escopo:

```
# no escopo global
error: 'PI' was not declared in this scope
# dentro de um namespace
error: 'PI' is not a member of 'n'
```

> **Correção de um erro que estava neste README.** Versões anteriores deste arquivo (e um
> comentário em `geometria.cppm`) descreviam isto como um **bug do GCC ligado ao global module
> fragment**, afirmando que "sem o global module fragment a mesma declaração funciona" e que
> "dentro de um namespace exportado funciona nos dois casos". A matriz acima, produzida
> rodando os dez casos, mostra que **as duas afirmações são falsas**: as duas colunas são
> idênticas, o GMF é irrelevante, e o namespace não resgata nada. Não há bug do GCC aqui —
> há uma regra do C++ (`constexpr` no escopo de namespace ⇒ linkage interna) que o modelo de
> módulos torna **visível** em vez de silenciosa. Num header, a mesma declaração compila e o
> sintoma é uma cópia por TU, sem aviso nenhum.

Ou seja: **este erro de compilação é uma boa notícia.** O módulo diagnosticou, em tempo de
compilação, algo que um header teria escondido.

### 2. `.cppm` precisa de `-x c++` no GCC

```bash
g++ -fmodules-ts -c geometria.cppm          # ✗
g++ -fmodules-ts -x c++ -c geometria.cppm   # ✓
```

O GCC não reconhece a extensão `.cppm` e trata o arquivo como entrada de **linker**. E o
sintoma é traiçoeiro — ele **não falha**:

```
g++: warning: geometria.cppm: linker input file unused because linking not done
```

É um *warning*, com código de saída **0**. O `make` segue em frente feliz, e o erro só aparece
no passo seguinte, dizendo que o `.o` não existe. O Clang reconhece `.cppm` sozinho.

### 3. O `.gcm` não entra no link, e é grande

Compilar uma interface produz **dois** artefatos com papéis diferentes:

```
geometria.o                 ← o código, para o linker
gcm.cache/geometria.gcm     ← a interface, para os próximos g++
```

```
-rw-r--r-- 3,3K  gcm.cache/geometria.gcm
-rw-r--r-- 2,1M  gcm.cache/formatacao.gcm      ← inclui <string> transitivamente
```

650× de diferença entre os dois. `formatacao` puxou `<string>` no GMF, e o `.gcm` serializa
tudo o que ficou visível. Não é desperdício — é o `<string>` analisado **uma vez** em vez de
uma vez por TU —, mas é bom saber por que o `gcm.cache/` cresce rápido.

⚠ Um `.gcm` é **específico** da versão do compilador e das flags. Trocar de GCC, ou mudar
`-std`, invalida o cache inteiro. Se aparecer um erro incompreensível de módulo, o primeiro
remédio é `make clean` (que apaga `gcm.cache/`).

### 4. Header units: `import <vector>;` precisa de pré-compilação

Além de módulos nomeados, existe a *header unit*: um header **antigo**, sem uma linha
modificada, pré-compilado e depois importado com a semântica de módulo (macros não vazam do
importador para dentro dele; os nomes que ele declara continuam todos visíveis, porque um
header não tem `export`). É a ponte de migração para código que você não pode reescrever.

```bash
g++ -std=c++20 -fmodules-ts -x c++-system-header vector   # pré-compila
```

```cpp
import <vector>;      // ⚠ em linha própria; `import <vector>; int main()` não compila
```

Funciona, mas exige pré-compilar **cada** header antes — o que devolve o problema de ordem.
O `import std;` (o módulo da biblioteca padrão inteira, que tornaria isso desnecessário)
**não** está disponível no GCC 13.

### 5. A ordem de compilação não é rastreável pelas ferramentas antigas

Um `#include` o Make rastreia sozinho com `-MMD`: o compilador emite a lista de arquivos que
colou. Um `import` **não** — a dependência é sobre o `.gcm`, que só existe **depois** de
compilar aquilo de que se depende. Não há como descobrir a ordem lendo os `.o`; é preciso ler
os **fontes** antes de compilar qualquer um. Isso se chama **scanner de dependências**: um
programa que lê todos os `.cppm`/`.cpp`, extrai as declarações `export module` e `import`,
monta o grafo e só então diz ao build system em que ordem compilar.

É por isso que este subprojeto declara as dependências à mão, no [`Makefile`](Makefile):

```make
formatacao.o: geometria.o        # o Make não descobre isso sozinho
main.o:       formatacao.o geometria.o
```

---

## Por que fora do Meson

O suporte a módulos no Meson com GCC 13 é experimental e frágil, e a causa é a armadilha 5: o
scanner precisa descobrir a ordem **antes** de compilar, e o formato que o GCC 13 emite não é
o que o Meson espera. O `-fdeps-format=p1689r5` — o formato padronizado de saída de scanner,
que o CMake 3.28+ consome — só chegou no GCC 14.

Manter este subprojeto no [`../meson.build`](../meson.build) quebraria o `make build` de quem
clonar o repositório. Um `Makefile` de 40 linhas demonstra o mesmo conteúdo — e, na verdade,
demonstra **melhor**, porque a ordem de compilação fica explícita em vez de escondida dentro
de uma ferramenta.

**É exatamente esse problema de ferramental que atrasou a adoção de módulos por cinco anos.**
A linguagem estava pronta em 2020; os build systems, não.

---

## Estado da adoção

| Compilador | Módulos nomeados | Partições | `module :private;` | Header units | `import std;` |
|---|---|---|---|---|---|
| **GCC 13** (esta máquina) | ✓ `-fmodules-ts` | ✓ | ✗ *unimplemented* | ✓ | ✗ |
| GCC 14+ | ✓ `-fmodules` | ✓ | parcial | ✓ | ✗ |
| Clang 16+ | ✓ | ✓ | ✓ | ✓ | parcial |
| MSVC 19.3x | ✓ | ✓ | ✓ | ✓ | ✓ |

As três primeiras colunas da linha do GCC 13 foram verificadas rodando os exemplos deste
README nesta máquina; as demais linhas vêm da documentação de cada compilador.

| Build system | Suporte |
|---|---|
| CMake 3.28+ | ✓ (exige Ninja 1.11+ e um compilador com scanner) |
| Meson | experimental |
| Make | manual, como aqui |

---

## Regras de ouro

| Situação | Faça |
|---|---|
| `#include` numa interface de módulo | só no global module fragment (`module;`) |
| Constante num módulo | `inline constexpr`, dentro do namespace exportado |
| Compilar `.cppm` no GCC | `-x c++` explícito — e desconfie de *warning*, não de erro |
| Ordem de compilação | interface **antes** de quem importa; sempre |
| Reexportar uma dependência | `export import` |
| Esconder um detalhe | simplesmente não exporte (mas veja a nuance do Demo 3) |
| Separar declaração de definição | unidade de implementação: `module NOME;` sem `export` |
| Quebrar um módulo grande | partições: `export module N:parte;` + `export import :parte;` |
| Erro de módulo incompreensível | `make clean` — o `gcm.cache/` provavelmente está velho |
| Adotar módulos hoje | só se o build system **e** o compilador mínimo suportarem |
| Migrar um projeto grande | comece pelas folhas do grafo de dependências |

---

## Como validar

```bash
cd src/modules
make clean          # apaga .o, gcm.cache/ e o executável
make                # deve imprimir os 4 comandos g++ na ordem acima
make run            # compila e executa
make mostrar-cache  # lista os .gcm gerados, com tamanho
```

O que se espera ver:

1. **`make`** imprime exatamente quatro linhas de `g++`, nesta ordem — `geometria.cppm`,
   `formatacao.cppm`, `main.cpp`, link — **sem nenhum warning**, apesar de
   `-Wall -Wextra -Wpedantic`.
2. **`make mostrar-cache`** lista dois arquivos: `geometria.gcm` (≈ 3,3 K) e
   `formatacao.gcm` (≈ 2,1 M).
3. **`make run`** imprime quatro blocos numerados (`1 · Usando um modulo importado` …
   `4 · O que isso custa e o que economiza`) e termina com:

   ```
   ✓ Todos os demos de modulos concluidos!
   ```

Três experimentos que provam o que o README afirma:

**1 · O isolamento é real.** Descomente a linha `double r = std::sqrt(16.0);` em
[`main.cpp`](main.cpp) e recompile:

```bash
make clean && make
#  → main.cpp: error: 'sqrt' is not a member of 'std'
```

**2 · A ordem é obrigatória.** Apague só o `.gcm` da dependência e force a recompilação de
quem depende dela:

```bash
make clean && make
rm gcm.cache/geometria.gcm && touch formatacao.cppm && make
```

Saída medida:

```
geometria: error: failed to read compiled module: No such file or directory
geometria: note: compiled module file is 'gcm.cache/geometria.gcm'
geometria: note: imports must be built before being imported
geometria: fatal error: returning to the gate for a mechanical issue
```

Guarde a terceira linha — *"imports must be built before being imported"*. É o subprojeto
inteiro resumido numa frase, dita pelo próprio compilador.

**3 · O `-x c++` não é decoração.** Repare que o comando **sem** ele sai com código **0**:

```bash
make clean
g++ -std=c++20 -fmodules-ts -c geometria.cppm -o geometria.o; echo "saída = $?"
#  → g++: warning: geometria.cppm: linker input file unused because linking not done
#  → saída = 0        ... e nenhum geometria.o foi criado
```

Um build que confia no código de saída passa direto por esse erro.

---

## Referências

Deste repositório:

- [`../linkage/`](../linkage/) — TU, ODR e linkage interna: os três termos que este README usa
  em toda página, tratados a fundo
- [`../compile_time/`](../compile_time/) — o que o compilador consegue fazer antes do runtime
- [`../meson/`](../meson/) — o build system da raiz, e por que módulos não cabem nele hoje
- [`../cmake/`](../cmake/) — o outro build system do repositório; CMake 3.28+ é hoje o caminho
  mais maduro para módulos
- [`../testing/`](../testing/) — o GoogleTest usado nos demais subprojetos
- [`../ROADMAP.md`](../ROADMAP.md) — onde este subprojeto entra na trilha

Fontes canônicas:

- **GCC — C++ Modules**: <https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b-Modules.html> — a
  única documentação que descreve o `gcm.cache/` e o `-fmodules-ts`
- **P1103R3, *Merging Modules***, Richard Smith — a proposta que entrou no C++20
- **cppreference — Modules**: <https://en.cppreference.com/w/cpp/language/modules> — a
  gramática completa (interface, implementação, partição, private fragment)
- **P1689R5, *Format for describing dependencies of source files*** — o formato de scanner
  citado em "Por que fora do Meson"
- **CMake — `CXX_MODULES`**: <https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html>
