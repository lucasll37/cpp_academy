# undefined_behavior — Comportamento Indefinido e Como se Defender

## O que é este projeto?

A definição do padrão é curta e implacável:

> *"behavior for which this document imposes no requirements"*

Nenhuma exigência. E — o ponto que quase todo mundo perde — o efeito **não precisa se limitar
ao trecho errado**. Ele pode aparecer *antes* da linha do erro, porque o compilador
reorganizou o código assumindo que o UB não aconteceria.

Este subprojeto tem **dois alvos**, e a distinção importa:

| alvo | o que é |
|---|---|
| `./dist/bin/undefined_behavior` | a **narrativa**. **Não executa UB.** Instalado. |
| `ub_exemplos` | contém UB **de propósito**, para rodar sob sanitizer. **Não instalado.** |

```bash
make build && ./dist/bin/undefined_behavior     # a narrativa

g++ -std=c++20 -O0 -g -fsanitize=address,undefined \
    src/undefined_behavior/ub_exemplos.cpp -o /tmp/ub
/tmp/ub          # lista os 12 casos
/tmp/ub uaf      # roda um, e veja o diagnóstico
```

**Nenhuma afirmação sobre o que os sanitizers pegam foi copiada de documentação.** Cada linha
da matriz da seção 3 saiu de rodar `ub_exemplos.cpp` neste toolchain — **GCC 13.3.0 (Ubuntu
24.04) salvo onde a tabela diz clang 18.1.3**. Três resultados contrariaram o senso comum.

---

## Antes de começar

O texto usa os termos abaixo o tempo todo. Nenhum deles é pressuposto.

**Comportamento indefinido (UB).** Situação em que o padrão **não impõe requisito nenhum** ao
que o programa faz. Não é "resultado desconhecido": é *nenhuma* obrigação — inclusive sobre o
que aconteceu **antes** da linha errada.

**TU** (*translation unit*, unidade de tradução). Um `.cpp` **depois** do pré-processador, isto
é, com todos os `#include` já colados dentro. É a unidade que o compilador vê de cada vez; o
*linker* junta as TUs no fim. Duas TUs não sabem nada uma da outra.

**ODR** (*One Definition Rule*, regra da definição única). Cada entidade com *linkage* externo
— função, variável, classe, template — pode ter **uma única definição** em todo o programa. Se
duas TUs definem a "mesma" coisa de formas **diferentes**, o programa é malformado e **nenhum
diagnóstico é exigido**: o linker escolhe uma das versões arbitrariamente e as duas metades do
programa passam a discordar sobre o layout. O caso canônico:

```cpp
// a.cpp                          // b.cpp
#define DEBUG                     // (sem DEFINE)
#include "s.hpp"                  #include "s.hpp"

// s.hpp
struct S { int x;
#ifdef DEBUG
           int contador;          // ← em a.cpp S tem 8 bytes; em b.cpp, 4
#endif
};
```

**lvalue.** Uma expressão que **designa um objeto** e da qual se pode tomar o endereço:
`x`, `v[3]`, `*p`, `obj.membro`. O contrário típico é um *prvalue*, que é um valor puro sem
identidade: `42`, `a + b`. A regra de *strict aliasing* (Demo 4) é enunciada em termos de
lvalue porque ela fala do **caminho pelo qual você acessa um objeto**.

**cv** (*cv-qualifier*). Abreviação padrão de `const` e/ou `volatile`. "O tipo exato, com ou sem
cv" quer dizer que `int`, `const int` e `volatile int` contam como o mesmo tipo para a regra.

**`volatile`.** Proíbe o compilador de **eliminar, duplicar ou reordenar** aquele acesso — ele
é *observável*. É o que se usa para registrador mapeado em memória e para impedir que um
`memset` de limpeza seja apagado. **Não** dá nenhuma garantia de atomicidade nem de ordem entre
threads; para isso existe `std::atomic` ([`src/concurrency2/`](../concurrency2/)).

**Trivially copyable.** Um tipo cujos bytes podem ser copiados com `memcpy` e continuar
valendo: sem construtor de cópia/movimento definido pelo usuário, sem destrutor não trivial,
sem membro virtual. `int`, `float`, `struct Ponto { double x, y; }` são; `std::string` não é.
É o pré-requisito de `std::bit_cast`.

**Type punning.** Ler os **mesmos bytes** como se fossem de outro tipo — por exemplo, examinar
o padrão de bits de um `float` como um inteiro de 32 bits. É uma necessidade legítima
(serialização, hashing, ponto flutuante); o problema é o **jeito** de fazer. Demo 4.

**Dead-store elimination.** Otimização que apaga uma escrita cujo valor **nunca é lido depois**.
Perfeitamente legal — e é por isso que o `memset` que zera uma senha antes de a variável sair de
escopo simplesmente desaparece do binário.

**Complemento de dois.** A representação de inteiros com sinal em que o bit mais alto vale
`-2^(n-1)`; `-1` é `0xFFFFFFFF`. Desde o C++20 (P0907) ela é **obrigatória** — antes disso o
padrão também admitia complemento de um e sinal-e-magnitude, o que é parte da razão histórica de
tanta coisa ser deixada indefinida.

**ABI** (*Application Binary Interface*). O contrato **binário** entre pedaços já compilados:
tamanho e layout de cada tipo, como argumentos são passados, como nomes são decorados. Duas
bibliotecas compiladas com opções que mudam o layout de um tipo têm ABIs incompatíveis — e o
linker **não** reclama. O resultado é corrupção silenciosa.

**Sanitizer.** Uma checagem inserida **em tempo de compilação** pelo próprio compilador, mais
uma biblioteca de runtime que a acompanha. É análise **dinâmica**: só vê o que de fato
executou. Demo 3 explica o mecanismo.

## Por que estudar isto?

Porque UB não é "um valor errado". É a **remoção de código que você escreveu** — e o exemplo
canônico não é acadêmico:

```c
struct sock* sk = tun->sk;      // ← desreferencia tun
if (!tun) return POLLERR;       // ← checa DEPOIS
```

O GCC raciocinou: `tun->sk` só é legal se `tun != nullptr`, logo `!tun` é sempre falso, logo o
`if` pode sumir. E sumiu. Virou o **CVE-2009-1897** no kernel Linux — escalação de privilégio.

### Seu primeiro UB, do início ao fim

Antes de qualquer teoria: escreva o erro, veja o diagnóstico, aprenda a lê-lo. Um arquivo
completo, nada omitido:

```cpp
// v.cpp
#include <cstdio>

int main() {
    int v[3]{};
    std::printf("%d\n", v[5]);      // ← fora dos limites: UB
}
```

Sem sanitizer isto costuma imprimir lixo e sair com 0 — o pior desfecho possível, porque parece
funcionar. Com sanitizer:

```bash
g++ -std=c++20 -O0 -g -fsanitize=address,undefined v.cpp -o /tmp/v
/tmp/v
```

Um relatório de ASan de verdade (este saiu de `/tmp/ub uaf`, o caso `caso_uaf` deste
subprojeto, porque ele mostra as **três** pilhas que interessam):

```
=================================================================
==142042==ERROR: AddressSanitizer: heap-use-after-free on address 0x502000000010 ...
READ of size 4 at 0x502000000010 thread T0
    #0 ... in caso_uaf(int) src/undefined_behavior/ub_exemplos.cpp:51
    #1 ... in main         src/undefined_behavior/ub_exemplos.cpp:142

0x502000000010 is located 0 bytes inside of 16-byte region [0x502000000010,0x502000000020)
freed by thread T0 here:
    #0 ... in operator delete[](void*)
    #1 ... in caso_uaf(int) src/undefined_behavior/ub_exemplos.cpp:50

previously allocated by thread T0 here:
    #0 ... in operator new[](unsigned long)
    #1 ... in caso_uaf(int) src/undefined_behavior/ub_exemplos.cpp:48

SUMMARY: AddressSanitizer: heap-use-after-free src/undefined_behavior/ub_exemplos.cpp:51 in caso_uaf(int)
Shadow bytes around the buggy address:
  0x501ffffffd80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x502000000000: fa fa[fd]fd fa fa fa fa fa fa fa fa fa fa fa fa
```

Como se lê, parte por parte:

| trecho | o que diz |
|---|---|
| `ERROR: AddressSanitizer: heap-use-after-free` | **qual** é o erro |
| `READ of size 4 ... thread T0` | foi uma **leitura** de 4 bytes, na thread principal |
| `#0 ... ub_exemplos.cpp:51` | **onde** — arquivo e linha, graças ao `-g` |
| `freed by thread T0 here` | **onde a memória foi liberada** (linha 50) |
| `previously allocated by ... here` | **onde ela foi alocada** (linha 48) |
| `SUMMARY:` | a linha única para colar num ticket ou casar num grep de CI |
| `Shadow bytes` | o mapa interno; `fd` = "liberado", `fa` = *redzone*, `00` = acessível |

O relatório do **UBSan** é bem mais curto — uma linha por achado, com arquivo, coluna e a
explicação:

```
src/undefined_behavior/ub_exemplos.cpp:72:16: runtime error: signed integer overflow:
2147483647 + 1 cannot be represented in type 'int'
```

Duas pilhas e uma linha: com isso já dá para consertar 90% dos casos.

## Estrutura de arquivos

```
src/undefined_behavior/
├── main.cpp                ← a narrativa, cor orange_red — NÃO executa UB
├── demo_catalogo.hpp       ← 1 · as 4 categorias, por que UB existe, o catálogo
├── demo_otimizador.hpp     ← 2 · como o UB licencia o otimizador
├── demo_sanitizers.hpp     ← 3 · a matriz de detecção MEDIDA
├── demo_aliasing.hpp       ← 4 · strict aliasing, bit_cast, union
├── demo_defesas.hpp        ← 5 · as cinco camadas
├── ub_exemplos.cpp         ← ⚠ 12 casos de UB REAL, um por argumento
├── meson.build
└── README.md
```

---

## Demo 1: O que é UB

### As quatro categorias

| categoria | o compilador… | exemplo |
|---|---|---|
| bem definido | faz o que o padrão diz | `2 + 2` |
| implementation-defined | escolhe **e documenta** | `sizeof(int)` |
| unspecified | escolhe, sem documentar | ordem de avaliação |
| **UNDEFINED** | **não tem obrigação nenhuma** | overflow de `int` |

As três primeiras são seguras: o programa faz *algo* previsível ou pelo menos limitado. A
quarta não tem limite.

Neste toolchain: `sizeof(long)` = 8 (**é 4 no Windows**), `char` é `signed` (**é `unsigned` em
ARM por padrão**).

### Por que UB existe — não é descuido

1. **Desempenho.** Se `i + 1 > i` pudesse ser falso, o compilador não poderia simplificar
   `for (int i = 0; i <= n; ++i)` nem promover `i` a um registrador de 64 bits.
2. **Portabilidade.** O padrão precisa valer para máquinas com complemento de 1 e palavras de
   9 bits.
3. **Custo zero para quem não usa.** Definir "acesso fora do vetor lança exceção" exigiria
   checagem em **todo** acesso.

> O C++20 converteu um caso famoso: inteiros com sinal passaram a ser obrigatoriamente
> complemento de dois (P0907). O **overflow** deles, no entanto, **continua sendo UB** — a
> representação foi fixada, o overflow não.

### O catálogo, por sanitizer que o pega

**Memória** (ASan): fora dos limites, use-after-free, use-after-scope, double free, `delete[]`
em ponteiro de `new`, `nullptr`, referência a objeto morto.

**Aritmética** (UBSan): overflow com **sinal** (sem sinal é definido!), divisão por zero,
`INT_MIN / -1`, shift ≥ largura do tipo, `double` → `int` fora da faixa.

**Tipos**: strict aliasing, ler o membro **inativo** de um `union` (**legal em C; em C++ é UB,
salvo a exceção da sequência inicial comum — ver Demo 4**), desalinhamento, usar objeto antes do
construtor ou depois do destrutor.

**Controle**: função não-`void` que termina sem `return`, data race, laço infinito sem efeito
observável, exceção escapando de destrutor ou de `noexcept`.

**Biblioteca**: `v[i]` com `i >= size()` (`.at(i)` lança), desreferenciar `end()` ou iterador
invalidado, `.front()` em container vazio, `memcpy` com `nullptr` mesmo com tamanho 0,
`string_view` sobrevivendo à string.

---

## Demo 2: Como o UB licencia o otimizador

O compilador raciocina assim, e a lógica é impecável:

> *"Este programa não tem UB. Logo, se `p` foi desreferenciado, `p != nullptr`. Logo, a
> checagem `if (p)` três linhas abaixo é sempre verdadeira. Logo, posso apagá-la."*

### A checagem de overflow que não checa

```cpp
if (a + b < a) trata_overflow();     // ❌ some com -O2
```

A forma correta, medida no demo:

```cpp
int r;
if (__builtin_add_overflow(a, b, &r)) trata_overflow();   // usa o flag do processador
// ou, sem extensão:
if (b > 0 && a > INT_MAX - b) trata_overflow();
```

### ⚠ MEDIDO: o mesmo UAF, detectado com `-O0` e invisível com `-O1`

```cpp
int caso_uaf(int n) {
    int* p = new int[4];
    p[0] = n;
    delete[] p;
    return p[0];            // ← use-after-free
}
```

Compilado com `-fsanitize=address`, o **mesmo** código:

```
-O0 → ERROR: AddressSanitizer: heap-use-after-free
-O1 → imprime 42, sem diagnóstico nenhum
```

Com `-O1` o compilador viu `p[0] = n; ... return p[0]` e propagou a constante: `return n`. **A
leitura desapareceu antes de o sanitizer poder instrumentá-la.** Não há acesso, logo não há o
que reportar.

Duas conclusões:

1. Rode a suíte sob sanitizer em `-O0` **e** em `-O1`/`-O2`. Cada nível esconde e revela coisas
   diferentes.
2. **"O ASan não acusou" não é prova de que não há UB.**

### Time travel

```cpp
void f(int* p, bool cond) {
    printf("antes\n");      // ← pode NÃO ser impresso
    if (cond) *p = 1;       // ← o UB está aqui
}
```

O padrão diz que a execução **inteira** fica indefinida — não a partir do UB, e sim desde o
começo. É o que torna a depuração tão desagradável: o último `printf` que apareceu não delimita
onde o problema está.

> Corolário: `std::unreachable()` (C++23) e `[[assume(x)]]` (C++23) são **UB deliberado** — as
> duas únicas formas de UB que você vai escrever **de propósito**.

```cpp
#include <utility>      // std::unreachable

int dobro_do_digito(int d) {
    switch (d) {
        case 0: return 0;
        case 1: return 2;
        case 2: return 4;
    }
    std::unreachable();          // "juro que d ∈ {0,1,2}" → o compilador
                                 //  apaga a checagem final e o return implícito
}

void escala(int* p, int n) {
    [[assume(n > 0 && n % 4 == 0)]];   // "juro que n é positivo e múltiplo de 4"
    for (int i = 0; i < n; ++i) p[i] *= 2;   // → vetoriza sem prólogo nem epílogo
}
```

> ⚠ São **promessas**, não verificações. Se a promessa for falsa, o programa tem UB — com todas
> as consequências desta seção. Antes de usá-las, meça: quase sempre o ganho não existe, e o
> risco sim. Nos compiladores anteriores ao C++23 os equivalentes são
> `__builtin_unreachable()` (GCC/Clang) e `__assume(x)` (MSVC).

### Outros casos famosos

- **`memset` para apagar senha** — não é UB, é *dead-store elimination* legítima, e a senha fica
  na memória. Use `explicit_bzero` ou um ponteiro `volatile`.
- **`if (this == nullptr)`** — chamar um método já pressupõe `this != nullptr`, então a checagem
  some (`-Wnonnull-compare`).
- **Função sem `return`** — na prática o compilador "continua" para a função seguinte no
  binário. Ligue `-Werror=return-type`.

### Do sintoma à causa

O README ensina as categorias de UB; o que você **vive** é o caminho inverso — um sintoma. Esta
tabela faz a ponte:

| sintoma | leitura provável |
|---|---|
| funciona em `-O0`, quebra em `-O1`/`-O2` | o otimizador usou uma **premissa de UB**: apagou uma checagem, propagou uma constante, vetorizou o laço. Comece por `-fsanitize=undefined` **nos dois níveis** |
| quebra **só** em release | ou o mesmo caso acima, ou um `assert` que sumiu com `NDEBUG` e estava segurando a invariante |
| passou a funcionar depois que eu **acrescentei um `printf`** | o layout de pilha ou de registradores mudou; um *overflow* passou a atingir outra variável. É estouro de buffer até prova em contrário — rode sob ASan |
| funciona no GCC, quebra no clang (ou vice-versa) | **não é bug de compilador**. É UB ou *unspecified* (ordem de avaliação, por exemplo). Compile com os dois, sempre |
| "funciona há cinco anos" | não é prova de nada. UB fica latente até a próxima versão do compilador, a próxima flag ou o próximo `-march` |
| o valor está certo em debug e errado em release, **sem** mudar o código | leitura de variável não inicializada, ou uso de objeto morto. `-Wmaybe-uninitialized`, `-fsanitize=address` |
| quebra só com mais de uma thread, e de forma diferente a cada vez | *data race* — que é UB. `-fsanitize=thread` ([`src/concurrency2/`](../concurrency2/)) |

---

## Demo 3: Os sanitizers, medidos

### Como um sanitizer funciona

Não é mágica nem emulação: é **instrumentação em tempo de compilação**. O compilador insere
checagens em torno de cada acesso, e uma biblioteca de runtime mantém o mapa do que é válido.

No ASan, esse mapa tem dois ingredientes:

- **Redzones.** Toda alocação ganha faixas **envenenadas** antes e depois. Um acesso que
  transborde por poucos bytes cai numa delas.
- **Shadow memory.** Para cada **8 bytes** de memória real existe **1 byte** de sombra que diz
  quantos daqueles 8 são acessíveis (e, se não são, por quê: `fa` = redzone, `fd` = liberado).
  A memória de sombra fica num intervalo fixo do espaço de endereços, alcançado por uma conta
  de uma instrução: `sombra = (endereco >> 3) + offset`.

Cada `load`/`store` do seu programa passa a ser: calcular o endereço da sombra, consultar,
e — se o byte de sombra reprovar — chamar o *reporter*. Três conclusões caem de graça daí:

1. **Por que custa ~2×**: cada acesso vira três ou quatro instruções em vez de uma, e o
   programa passa a tocar duas regiões de memória (o dado e a sombra), o que dobra a pressão de
   cache.
2. **Por que ele não vê violação de strict aliasing**: o endereço é **válido** e a sombra
   aprova. O que está errado é o **tipo** pelo qual o acesso foi feito, e disso a sombra não
   sabe nada.
3. **Por que ASan e TSan são incompatíveis**: os dois querem instrumentar exatamente o mesmo
   `load`/`store`, com mapas de sombra diferentes.

O UBSan é outra coisa: em vez de mapa de memória, ele insere **a checagem específica** do erro
que aquela operação pode ter (o overflow no `add`, a largura do shift, o alinhamento do
`load`). Por isso custa quase nada — e por isso só pega o que foi instrumentado.

| sanitizer | flag | pega | custo |
|---|---|---|---|
| Address | `-fsanitize=address` | memória | ~2× |
| Undefined | `-fsanitize=undefined` | UB aritmético | ~1.2× |
| Thread | `-fsanitize=thread` | data races | 5–15× |
| Memory | `-fsanitize=memory` | não inicializado | ~3× |

> ⚠ ASan e TSan são **incompatíveis** entre si. Dois jobs no CI.
>
> ⚠ MSan exige que **toda** a pilha esteja instrumentada, inclusive a libstdc++. Sem uma libc++
> compilada com MSan, ele produz falsos positivos ou — como medido aqui — **simplesmente não
> reporta**. É o menos usável dos quatro, e o único que o GCC não implementa.

### A matriz de detecção

Rodando `ub_exemplos.cpp` com `-fsanitize=address,undefined`:

| caso | `-O0` | `-O1` |
|---|---|---|
| heap | ASan: buffer-overflow | ASan: buffer-overflow |
| pilha | UBSan: index out of bounds | idem |
| **uaf** | **ASan: heap-use-after-free** | **⚠ NADA** |
| dangling | UBSan: null reference binding | idem |
| overflow | UBSan: signed integer overflow | idem |
| shift | UBSan: shift exponent too large | idem |
| divisao | UBSan: division by zero | idem |
| nulo | UBSan: load of null pointer | idem |
| vazamento | LSan: detected memory leaks | idem |
| **aliasing** | **⚠ NADA** | **⚠ NADA** |
| **naoinit** | **⚠ NADA** | **⚠ NADA** |
| **alinhamento** | **⚠ NADA (GCC)** | **⚠ NADA (GCC)** |

**Três resultados que contrariam o senso comum:**

1. **`uaf` some com `-O1`** — explicado acima.
2. **`alinhamento` não é pego pelo GCC**, nem com `-fsanitize=alignment` isolado. O **clang**
   pega: `runtime error: load of misaligned address 0x... for type 'int', which requires 4 byte
   alignment`. Se alinhamento importa no seu código, use clang para esta checagem.
3. **`aliasing` não é pego por nenhum dos dois.** É a categoria de UB sem rede de proteção.

### ⚠ O que nenhum sanitizer pega

- **Strict aliasing** — não há instrumentação para isso.
- **Leitura de não inicializado**, na prática (MSan é inviável sem stdlib instrumentada). A
  defesa real é `-Wmaybe-uninitialized` e sempre inicializar.
- **Violação de ODR entre TUs** — só `-flto -Wodr` tem chance ([`src/linkage/`](../linkage/)).
- **A lógica errada** — é para isso que existem testes ([`src/testing/`](../testing/)).
- **O caminho que não foi executado.** Sanitizer é análise **dinâmica**: só vê o que rodou. Um
  `catch` que os testes nunca exercitam pode ter UB e passar limpo. Por isso sanitizer +
  cobertura formam um par: um mede o que rodou, o outro diz o que não rodou.

### Como rodar

```bash
g++ -std=c++20 -O1 -g -fsanitize=address,undefined \
    -fno-omit-frame-pointer -fno-sanitize-recover=all arq.cpp -o prog
```

`-fno-sanitize-recover=all` é **essencial no CI**: sem ele, o UBSan só imprime e continua, e o
exit code fica 0 — a suíte passa com o UB detectado.

```bash
meson setup build-asan -Db_sanitize=address,undefined -Db_lundef=false
meson test -C build-asan
```

```bash
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
ASAN_OPTIONS=detect_stack_use_after_return=1
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
```

---

## Demo 4: Strict aliasing

A regra: acessar o valor de um objeto por um lvalue de tipo **incompatível** é UB. As exceções:

- o tipo exato (com ou sem cv);
- `signed` ↔ `unsigned` do mesmo tipo;
- uma classe **base**;
- **`char`, `unsigned char` e `std::byte`** — os únicos universais.

> ⚠ `signed char` **não** está na lista dos universais.

Ela existe para o otimizador: se um `float*` não pode apontar para um `int`, escrever no float
não invalida o `int` guardado em registrador.

### O errado e o certo

```cpp
int i = *reinterpret_cast<const int*>(&f);      // ❌ UB — e NENHUM sanitizer avisa

std::memcpy(&i, &f, sizeof f);                  // ✓ legal desde sempre
auto i = std::bit_cast<std::uint32_t>(f);       // ✓ C++20
```

Medido: os dois dão `0x4048F5C3` para `3.14f`. E `memcpy` **não é lento** — com `-O2` o
compilador reconhece o padrão e emite um `mov`.

`bit_cast` é melhor que `memcpy` em quatro pontos: exige `sizeof` igual (erro de compilação se
não for), exige os dois tipos trivially copyable, não menciona ponteiro (não há como errar o
`sizeof`), e é **`constexpr`**:

```cpp
static_assert(std::bit_cast<std::uint32_t>(1.0f) == 0x3F800000u);   // passa em compilação
```

### ⚠ `union`: legal em C, UB em C++

Ler o membro **inativo** de um `union` é o idioma padrão de type punning em C desde o C99. Em
C++ é UB — só um membro existe por vez. GCC e Clang aceitam como extensão; o MSVC não dá a mesma
garantia. **Use `bit_cast`.**

### `-fno-strict-aliasing`

O kernel Linux compila com essa flag desde 2003 — não porque a regra seja ruim, mas porque
auditar milhões de linhas de C antigo é inviável.

Ordem de preferência: **`bit_cast`/`memcpy`** > `char`/`std::byte` quando você precisa mesmo ver
bytes > `__attribute__((may_alias))` cirúrgico > `-fno-strict-aliasing` no projeto (o martelo).

> ⚠ Se usar a flag, ela precisa valer para **todas** as TUs. Misturar é a violação de ODR
> silenciosa de [`src/linkage/`](../linkage/).

---

## Demo 5: As cinco camadas de defesa

Da mais barata para a mais cara — e a mais barata é também a mais eficaz:

### 1. Escrever código que não tem como ter UB (custo zero)

| em vez de… | use… |
|---|---|
| `new`/`delete` | `unique_ptr`, `vector` |
| `T*` + tamanho | `std::span` |
| array C | `std::array` |
| `v[i]` em código duvidoso | `v.at(i)` |
| `reinterpret_cast` | `std::bit_cast` |
| `int` para tamanhos | `std::size_t` |
| `strcpy`/`sprintf` | `std::string`, `std::format` |
| `T*` que pode ser nulo | `std::optional<T>`, `T&` |
| `union` para punning | `std::variant` / `bit_cast` |

`std::span` elimina a categoria inteira de "ponteiro e tamanho desencontrados": `soma_c(v.data(),
100)` compila sem reclamar; `soma_span(v)` não tem onde errar o tamanho.

### 2. Os avisos do compilador (custo zero)

Além de `-Wall -Wextra -Wpedantic`: `-Wshadow`, `-Wconversion`, `-Wsign-conversion`,
`-Wold-style-cast`, `-Wnull-dereference` (o caso do CVE do kernel), `-Wnon-virtual-dtor`,
`-Wduplicated-cond`, `-Wlogical-op`.

Os que devem ser **erro**: `-Werror=return-type`, `-Werror=uninitialized`,
`-Werror=implicit-fallthrough`.

### 3. Análise estática

`g++ -fanalyzer`, `clang-tidy --checks='bugprone-*,cert-*'`, `cppcheck --enable=all`. Encontra o
caminho que os testes não executaram — o ponto cego do sanitizer.

### 4. Sanitizers nos testes — a seção 3.

### 5. Hardening em produção

`-D_GLIBCXX_ASSERTIONS` (verifica `v[i]`, `.front()`), `-D_FORTIFY_SOURCE=3`,
`-fstack-protector-strong`, `-fcf-protection`.

> ⚠ **Atenção de ABI:** `_GLIBCXX_ASSERTIONS` muda o layout de alguns tipos. Precisa valer para
> **todas** as unidades e bibliotecas linkadas — misturar é corrupção silenciosa.

### ⚠ A parte honesta

Mesmo com todas as camadas, C++ não é uma linguagem segura por construção. Elas reduzem a
probabilidade; não a zeram. Isso não é argumento para abandonar C++ — é argumento para levar as
camadas a sério, e para reconhecer que **"eu tomo cuidado" nunca foi uma delas**.

---

## Como validar

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/undefined_behavior/main.cpp -o /tmp/ubm -L$FMTP/lib -lfmtd

# a narrativa, ela própria sob sanitizer — deve dar ZERO achados
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/undefined_behavior/main.cpp -o /tmp/ubmsan -L$FMTP/lib -lfmtd && /tmp/ubmsan
```

Estado atual: sem warnings com `-Wall -Wextra -Wpedantic`, e **0 achados reais sob ASan+UBSan**.

> Nota de método: um `grep` ingênuo por `runtime error` na saída acusa 5 ocorrências — que são
> o **texto dos próprios demos**, citando mensagens de sanitizer. O marcador real é
> `==PID==ERROR` ou `SUMMARY: AddressSanitizer`, e desses há zero.

`ub_exemplos.cpp` é o único arquivo do repositório que **contém UB de propósito**. Ele não é
instalado, é fixado em `-O0` no `meson.build`, e cada caso exige um argumento explícito.

## Referências

- [`src/testing/`](../testing/) — sanitizers nos testes, e a cobertura que diz o que não rodou
- [`src/linkage/`](../linkage/) — a violação de ODR que nenhum sanitizer pega
- [`src/concurrency2/`](../concurrency2/) — data race é UB; o TSan e o modelo de memória
- [`src/bits/`](../bits/) — `bit_cast`, promoção inteira, shift
- [`src/layout/`](../layout/) — alinhamento, e por que `memcpy` do tipo errado corrompe
- Chris Lattner, *What Every C Programmer Should Know About Undefined Behavior* (LLVM blog)
- John Regehr, *A Guide to Undefined Behavior in C and C++*
- CVE-2009-1897 — o `if` removido no `tun_chr_poll` do kernel Linux
