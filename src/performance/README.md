# performance — Cache, Layout e Como Medir

## O que é este projeto?

Este subprojeto **mede**. Nada aqui é afirmado sem um número obtido na própria execução, e três
das medições contrariaram o texto que eu tinha escrito antes de medir — nos três casos o texto
estava errado, e a explicação era mais interessante que a expectativa.

A distância entre o processador e a RAM é a maior assimetria do hardware moderno. Em ciclos:

```
registrador ....... 0
cache L1 .......... 4
cache L2 ......... 12
cache L3 ......... 40
RAM ............. 200      ← 50× mais lento que L1
```

Em 200 ciclos o processador executaria centenas de instruções. Por isso, em código que percorre
dados, **"quantas operações" quase nunca é a pergunta certa — "quantos cache miss" é**.

```bash
make build && ./dist/bin/performance    # leva alguns segundos, aloca ~500 MB
```

## Antes de começar: como a memória funciona

Todo este subprojeto depende de entender **por que ler da RAM é caro**.

### A hierarquia, e a assimetria que ela esconde

```
        registradores    ~0 ciclos      ~128 bytes no total
             ↕
        cache L1         ~4 ciclos      ~32 KB por núcleo
             ↕
        cache L2        ~12 ciclos      ~256 KB - 1 MB
             ↕
        cache L3        ~40 ciclos      ~8-32 MB, COMPARTILHADO
             ↕
        RAM            ~200 ciclos      gigabytes
```

Em 200 ciclos o processador executaria **centenas** de instruções. Por isso, em
código que percorre dados, *"quantas operações"* quase nunca é a pergunta certa
— **"quantos acessos vão até a RAM"** é.

### A linha de cache

O processador **nunca lê 4 bytes** da RAM. Ele lê uma **linha**: 64 bytes
contíguos, no x86-64. Ler `v[0]` traz `v[0..15]` junto, de graça.

```
RAM:  [ ..... 64 bytes ..... ][ ..... 64 bytes ..... ]
              ↓ uma linha inteira sobe para o cache
cache:[ v[0] v[1] ... v[15] ]
```

Duas consequências que explicam quase tudo o que este subprojeto mede:

1. **acesso sequencial é quase de graça** — 15 dos 16 elementos já estão lá;
2. **acesso espalhado paga tudo** — cada elemento traz 64 bytes e usa 4.

### Latência ≠ banda

| | o que é | como se mede |
|---|---|---|
| **latência** | quanto tempo até o **primeiro** byte chegar | ciclos por acesso |
| **banda** | quantos bytes por segundo no **regime** | GB/s |

O *prefetcher* — um circuito que detecta padrões de acesso e busca as linhas
**antes** de serem pedidas — esconde a **latência**, não a banda. É por isso que
o demo 2 mede uma razão de 10× em bytes e só 1,5× em tempo.

### Coerência de cache

Cada núcleo tem o **seu** L1. Se dois núcleos guardam a mesma linha e um escreve,
o protocolo (MESI) **invalida** a cópia do outro, que precisa buscá-la de novo.

Isso acontece por **linha**, não por variável — e é a causa do *false sharing*
do demo 3, onde duas threads que mexem em variáveis **diferentes** ficam
11,7× mais lentas.

## Os números, medidos nesta máquina

| Experimento | Resultado |
|---|---|
| matriz 2048², por linha vs por coluna | **35×** |
| soma sequencial vs aleatória (8 M ints) | **26×** |
| false sharing: mesma linha vs `alignas(64)` | **11.7×** |
| `std::vector` vs `std::list` (2 M ints) | **6.6×** |
| AoS vs SoA, laço que usa 1 campo de 10 | **1.5×** (bytes: 10×) |
| laço com resultado descartado vs consumido | **∞** — foi eliminado |

E dois que **não** reproduziram o esperado:

| Experimento | Esperado | Medido |
|---|---|---|
| vetor ordenado vs embaralhado, `-O2` | ~3× | **1.0×** — o GCC gera `cmov` |
| AoS vs SoA usando todos os campos | AoS ganha | **SoA ganhou 1.6×** |

> Os valores absolutos variam com o processador, a carga e o compilador. O que **não** varia é a
> ordem de grandeza entre os casos comparados.

## Estrutura de arquivos

```
src/performance/
├── main.cpp                   ← ponto de entrada, cor dark_turquoise
├── bench.hpp                  ← sumidouro + mediana: o mínimo para não se enganar
├── demo_cache.hpp             ← 1 · linha de cache, linha vs coluna, vector vs list
├── demo_aos_soa.hpp           ← 2 · AoS vs SoA, nos dois padrões de acesso
├── demo_false_sharing.hpp     ← 3 · o bug de 11.7× em código correto
├── demo_branch.hpp            ← 4 · predição de desvio, e por que sumiu
├── demo_metodo.hpp            ← 5 · como medir sem se enganar
├── meson.build                ← fixa -O2 -g (ver abaixo)
└── README.md
```

> ⚠ Este é o único subprojeto que **fixa a otimização** no `meson.build`. O padrão do Meson vem
> do `buildtype` (debug → `-O0`), e a seção 4 mede um efeito que aparece em `-O0` e desaparece
> em `-O1`. Um benchmark sem otimização não diz nada sobre o binário que se entrega.

---

## Demo 1: Localidade de cache

### A linha de cache, medida

O processador nunca lê 4 bytes da RAM. Ele lê uma **linha** — 64 bytes no x86-64.

Somando 1 milhão de elementos de um vetor de 64 MB, variando o passo:

| passo | bytes entre acessos | tempo |
|---|---|---|
| 1 | 4 | 0.38 ms (1.0×) |
| 4 | 16 | 0.36 ms (0.9×) |
| 8 | 32 | 0.61 ms (1.6×) |
| **16** | **64** | **1.39 ms (3.7×)** |
| 32 | 128 | 2.48 ms (6.5×) |

O **joelho** está no passo 16 — exatamente `64 / sizeof(int)`. Até ali vários acessos
compartilham a mesma linha; a partir dali, cada acesso paga uma linha inteira. O passo 32 é
ainda pior por causa do **prefetcher**: ele busca linhas adjacentes, e com 128 B de passo metade
do que ele traz nunca é usada.

> ⚠ **Nota de método.** A primeira versão ia até o passo 1024 e afirmava que o tempo "estabiliza"
> depois de 16. A medição desmentiu as duas coisas:
>
> ```
> passo  256 → 1.75 ms   ← CAIU
> passo  512 → 1.56 ms   ← caiu mais
> passo 1024 → 4.05 ms   ← disparou
> ```
>
> A causa é um defeito do **experimento**: o `if (i >= kN) i = 0` faz o índice dar a volta. Com
> passo 256, só 65 536 posições distintas são tocadas — 4 MB, que **cabem no L3**. O benchmark
> deixou de medir latência de RAM e passou a medir L3. (O salto em 1024 é o tamanho da página:
> pressão sobre o TLB.)
>
> A tabela para no passo 32 porque é a faixa em que o experimento mede o que diz medir.
> **Um benchmark que produz um número não produziu, com isso, um número certo.**

### Percurso por linha vs por coluna — 35×

Matriz 2048×2048 (16 MB), somando **todos** os elementos:

```
por linha  (i, depois j) :  0.44 ms
por coluna (j, depois i) : 15.54 ms   ← 35× mais lento
```

Mesmo número de somas, mesma complexidade O(n²). A única diferença é a **ordem** dos acessos.
Por coluna, cada acesso salta 8192 bytes: uma linha de cache nova por elemento, e 15 dos 16
`int` trazidos são descartados.

É a razão de BLAS e Eigen fazerem **tiling** — quebrar a matriz em blocos que cabem em L1.

### `vector` vs `list` — 6.6×

```
std::vector<int> : 0.43 ms      4 bytes/elemento, contíguo
std::list<int>   : 2.80 ms     20 bytes/elemento, uma alocação por nó
```

O `std::list` tem inserção O(1) no meio — e ainda assim o `vector` costuma ganhar até nesse
caso: o `memmove` de N elementos contíguos é mais rápido que **percorrer** a lista até a posição.
É a palestra de Bjarne Stroustrup em Going Native 2012, e continua valendo.

### O prefetcher — 26×

```
crescente   :  0.97 ms  (1.0×)
decrescente :  1.04 ms  (1.1×)   ← o prefetcher também prevê para trás
aleatória   : 25.69 ms  (26.4×)
```

O prefetcher detecta passo constante, em qualquer direção, e busca as linhas antes de serem
pedidas. Acesso aleatório não tem padrão: cada miss é pago integralmente.

> É por isso que uma tabela hash com sondagem linear ganha de uma com encadeamento — não por
> fazer menos operações, mas por o prefetcher conseguir ajudar.

---

## Demo 2: AoS vs SoA

```cpp
// AoS — legível, e o padrão
struct Particula { float x, y, z, vx, vy, vz, massa, carga; int id; bool ativa; };
std::vector<Particula> mundo;

// SoA — o layout que a máquina prefere para alguns laços
struct Mundo { std::vector<float> x, y, z, vx, vy, vz, massa, carga; };
```

### Laço que usa poucos campos

Somando **apenas** `massa` de 4 M partículas:

```
AoS : 3.72 ms      trouxe 152 MB da RAM (a struct inteira, 40 B por partícula)
SoA : 2.47 ms      trouxe  15 MB        (só o vetor de massa)
```

A razão de **bytes** é 10×, e a de **tempo** é só 1.5×. A diferença entre esses dois números é o
**prefetcher**: o acesso do AoS continua sequencial, e o hardware busca as linhas antes de serem
pedidas. Ele não elimina o tráfego — elimina a **espera** por ele, até saturar a banda.

> "Contar bytes" prevê o **teto**, não o tempo.

### ⚠ Laço que usa todos os campos — o resultado que contrariou a hipótese

```
AoS : 8.62 ms
SoA : 5.39 ms      ← o SoA ainda ganhou (1.60×)
```

A hipótese era que, usando quase todos os campos, o AoS venceria. Não venceu. Duas razões
prováveis nesta máquina:

- `sizeof(Particula)` é **40**, e `64 / 40 = 1`: cada partícula já ocupa mais de uma linha, então
  o AoS não colhe o benefício de "tudo na mesma linha";
- o laço SoA **vetoriza**; o AoS, com passo de 40 bytes, não.

Para o AoS ganhar, a struct precisaria caber confortavelmente numa linha — o que remete a
[`src/layout/`](../layout/): **reordenar campos e cortar padding é pré-requisito, não detalhe**.

> **A moral:** SoA não é "o layout rápido". Ele troca **desperdício de linha** por **mais fluxos
> simultâneos**. O prefetcher acompanha um número limitado de streams (8 a 16); oito vetores em
> paralelo já disputam esse recurso.

### O meio-termo

SoA custa legibilidade: some a entidade como objeto. Em ordem de esforço:

1. **Separar por temperatura** — campos quentes numa struct, frios em outra. Mantém a
   legibilidade e resolve a maior parte.
2. **AoSoA** — blocos de N em SoA, num array: `struct Bloco { float x[8], y[8], z[8]; }`.
   Vetoriza bem e mantém localidade. É o layout de engines de física e HPC.
3. Uma **visão** sobre o SoA — proxy que se parece com `Particula&`.

E a regra que vale antes de tudo: **só reorganize depois que o profiler apontar aquele laço**.

---

## Demo 3: False sharing — 11.7×

Duas threads, duas variáveis **diferentes**, nenhum mutex, nenhum data race. E o programa fica
11.7× mais lento.

A unidade de coerência de cache não é a variável — é a **linha**. Se `a` e `b` caem na mesma
linha e o núcleo 0 escreve em `a`, o protocolo MESI invalida a linha no núcleo 1 — que precisa
buscá-la de novo para ler `b`. Os dois trocam a mesma linha, ida e volta, milhões de vezes.

```
4 threads × 20 M incrementos, cada uma no SEU contador:
  contadores na MESMA linha : 336.32 ms
  contadores SEPARADOS      :  28.72 ms   ← 11.7× mais rápido
```

**O código está correto nos dois casos.** A única diferença é o `alignas(64)`.

### As correções, em ordem

1. **A melhor: não compartilhar.** Acumule numa variável local e escreva uma vez no fim — o
   `++local` vira um registrador, sem tocar em memória.
2. `alignas(std::hardware_destructive_interference_size)`, quando o compartilhamento é
   inevitável. Custo: 32 → 256 bytes para 4 contadores (irrelevante ali, proibitivo em milhões).
3. Padding manual, se precisar de portabilidade estrita.

### ⚠ Onde ele se esconde

- `std::atomic<long> acessos[kThreads];` — contadores por thread num array.
- Uma struct com campos "independentes": `{ atomic<size_t> cabeca; atomic<size_t> cauda; }` — é
  por isso que a `FilaSPSC` de [`src/concurrency2/`](../concurrency2/) separa os dois com
  `alignas`.
- `std::vector<bool>` escrito por várias threads — aqui é **pior**: os bits compartilham bytes,
  então é **data race de verdade**, UB e não só lentidão.

**Diagnosticar, não adivinhar:** `perf c2c record` + `perf c2c report` foi feito exatamente para
isto — mostra quais linhas estão pingando entre núcleos.

---

## Demo 4: Predição de desvio — o experimento que não reproduz mais

O experimento clássico: `for (int x : v) if (x >= 128) s += x;` sobre 8 M valores aleatórios,
ordenados vs embaralhados.

| nível | ordenado | embaralhado | razão | `cmov` no binário |
|---|---|---|---|---|
| `-O0` | 41.50 ms | 69.31 ms | **1.7×** | 18 |
| `-O1` | 3.40 ms | 3.41 ms | 1.0× | 45 |
| `-O2` | 3.39 ms | 3.42 ms | 1.0× | 36 |
| `-O3` | 1.38 ms | 1.38 ms | 1.0× | 52 |

O efeito aparece em `-O0` e **desaparece a partir de `-O1`**. A causa está na última coluna: com
otimização, o GCC 13 converte o `if` em `cmov` — movimento condicional, **sem desvio**. Não há
aposta, logo não há como errar a aposta.

Esta é a resposta mais votada de todo o Stack Overflow (*"Why is processing a sorted array
faster than an unsorted array?"*, 2012) — e, no compilador de hoje com otimização ligada, **ela
não reproduz mais**.

Duas lições, e a segunda é a que vale:

1. o compilador moderno já elimina boa parte dos desvios imprevisíveis por conta própria;
2. **um benchmark medido em `-O0` não diz nada sobre o programa que você vai entregar.**

### Código sem desvio

```
(a) if (x >= 128) s += x;      3.42 ms
(b) s += x & -(x >= 128);      3.40 ms  (0.99×)
(c) s += x * (x >= 128);       3.35 ms  (0.98×)
```

Os três são iguais — porque o compilador já fazia. Antes de reescrever `if` como aritmética,
confirme:

```bash
g++ -O2 -S -masm=intel arq.cpp -o - | grep -c cmov
```

As dicas explícitas (`[[likely]]`, `[[unlikely]]`, `__builtin_expect`) ajudam quando você
**sabe** a proporção. Chutar errado piora — e o **PGO** (`-fprofile-generate`/`-fprofile-use`)
mede em vez de chutar.

### ⚠ Quando não se preocupar

A predição só importa quando três coisas coincidem: o desvio está num laço muito quente, a
condição é genuinamente imprevisível, **e** o corpo é curto (se ele faz trabalho de verdade, os
~15 ciclos desaparecem no meio).

**Medir:** `perf stat -e branches,branch-misses ./prog`. Abaixo de 1% é normal; acima de 5% num
laço quente, investigue; acima de 20%, você achou.

---

## Demo 5: Como medir sem se enganar

### Os cinco jeitos de produzir um número falso

**(1) O código foi eliminado.** Medido:

```
somando 10 M ints, resultado DESCARTADO :  0.000 ms
o mesmo, com `nao_otimize(s)`           :  1.317 ms
→ o primeiro foi ELIMINADO: ~20000× "mais rápido" porque não aconteceu
```

O sumidouro:

```cpp
template <typename T>
inline void nao_otimize(T const& valor) {
    asm volatile("" : : "r,m"(valor) : "memory");
}
```

Ele diz ao compilador: *"esta função leu `valor` e pode ter mexido em qualquer memória"*. É o
mesmo truque de `benchmark::DoNotOptimize`.

**(2) A primeira execução paga a mais** — page faults, cache frio, TLB frio. Sempre aqueça.

**(3) Uma amostra é ruído.** Uma interrupção do SO distorce a **média** e não move a **mediana**.
Use a mediana de 5 a 11 execuções.

**(4) O nível de otimização errado** — a seção 4 é o caso.

**(5) O dado é amigável demais** — a seção 1.1 é o caso, e o erro foi meu.

### Benchmark ou profiler?

```
benchmark → "a versão B é mais rápida que a A?"
profiler  → "ONDE o programa gasta o tempo?"
```

Profiler **primeiro**, para achar o trecho que importa; benchmark **depois**, para comparar
alternativas naquele trecho.

Otimizar sem profiler é o erro mais caro que existe — não porque a otimização não funcione, mas
porque ela se aplica ao lugar errado. Acelerar em 10× uma função que consome 2% do tempo melhora
o total em **1.8%**.

### `perf`, o mínimo

```bash
perf stat ./prog                 # IPC, branch-misses, cache-misses
perf record -g ./prog && perf report
perf annotate                    # linha a linha, com assembly
perf c2c record ./prog           # false sharing (seção 3)
```

`IPC` abaixo de 1.0 = esperando memória; acima de 2.0 = bom.

> ⚠ Compile com `-g` **mesmo em release**, e com `-fno-omit-frame-pointer`, ou o `perf report`
> mostra endereços em vez de nomes. `-g` não deixa o binário mais lento: só maior. (É o que o
> `meson.build` deste subprojeto faz.)
>
> ⚠ Em WSL e contêineres: `sudo sysctl kernel.perf_event_paranoid=1`, e alguns eventos de
> hardware não existem em VM.

### ⚠ A ordem certa de otimizar

1. **O algoritmo.** O(n log n) no lugar de O(n²) ganha de qualquer micro-otimização.
2. **Fazer menos.** O trabalho mais rápido é o que não acontece.
3. **O layout dos dados.** É onde estão os fatores de 10× e 35× medidos aqui. **Antes** de mexer
   em instrução.
4. **Paralelizar.** Limitado pelo número de núcleos, e acrescenta uma classe inteira de bugs.
5. **Micro-otimização.** 10 a 30%, custo alto de legibilidade, e o compilador frequentemente já
   fez.

E as flags, que são de graça: `-O2` (padrão sensato), `-O3` (**meça**, às vezes piora),
`-march=native` (⚠ o binário não roda em CPU mais antiga), `-flto`, e **PGO** — o ganho mais
consistente (5–15%) e quase nunca usado.

> **MEÇA, MUDE, MEÇA DE NOVO.** Sem o primeiro passo você não sabe onde mexer; sem o terceiro,
> não sabe se melhorou.

---

## Como validar

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -O2 -g -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/performance/main.cpp -o /tmp/pf -L$FMTP/lib -lfmtd -pthread
/tmp/pf
```

Estado atual: sem warnings com `-Wall -Wextra -Wpedantic`. O executável leva alguns segundos e
aloca algumas centenas de MB — os conjuntos precisam ser maiores que o L3 para os números
significarem alguma coisa.

## Referências

- [`src/layout/`](../layout/) — padding, `alignas`, e por que `sizeof` da struct decide o resultado
- [`src/concurrency2/`](../concurrency2/) — a `FilaSPSC` que separa os índices por causa da seção 3
- [`src/chrono/`](../chrono/) — como cronometrar: relógios, aquecimento, `steady_clock`
- [`src/ecs/`](../ecs/) — SoA como arquitetura, não como otimização
- [`src/stl/`](../stl/) — a escolha de container, do lado da complexidade
- Ulrich Drepper, *What Every Programmer Should Know About Memory* (2007) — ainda a referência
- Brendan Gregg, *Systems Performance* e os flame graphs
- Agner Fog, *Optimizing software in C++* — as tabelas de latência de instrução
