# value_semantics — Semântica de Valor, Move e Forwarding

## O que é este projeto?

Guia sobre o **modelo de valor do C++**: como o compilador classifica expressões, quando ele
copia, quando move, quando não faz nada — e como escrever tipos que participam disso
corretamente.

É o subprojeto que sustenta todos os outros. `std::move` aparece em dezenas de arquivos deste
repositório; aqui se explica o que ele realmente faz (spoiler: um `static_cast`).

Para rodar (a partir da **raiz do repositório**, `/home/lima/Projects/cpp_academy`):

```bash
make build                    # configura (se preciso), compila e instala em ./dist
./dist/bin/value_semantics    # o executável instalado
```

São 5 blocos com contadores instrumentados: cada afirmação deste README é **medida** pelo
programa, não apenas alegada. Ele termina com
`✓ Todos os demos de semântica de valor concluídos!`.

---

## Antes de começar

Esta seção define **todo** termo usado adiante. Nenhuma sigla aparece no texto sem estar aqui.

### Copiar vs mover, num programa completo

Antes de qualquer sutileza, o caso simples rodando. Salve como `copiavsmove.cpp`:

```cpp
#include <fmt/core.h>
#include <vector>
#include <chrono>

struct Buffer {
    std::vector<int> dados;
    explicit Buffer(std::size_t n) : dados(n, 7) {}

    Buffer(const Buffer& o) : dados(o.dados)              // CÓPIA: aloca e copia tudo
    { fmt::print("  CÓPIA: {} ints copiados\n", dados.size()); }

    Buffer(Buffer&& o) noexcept : dados(std::move(o.dados))  // MOVE: rouba o ponteiro interno
    { fmt::print("  MOVE : 0 ints copiados (ponteiro trocado)\n"); }
};

template <class F> long long micros(F f) {
    auto t0 = std::chrono::steady_clock::now();
    f();
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - t0).count();
}

int main() {
    Buffer b{1'000'000};                                       // 4 MB de ints
    auto tc = micros([&]{ Buffer copia  = b;             (void)copia;  });
    auto tm = micros([&]{ Buffer movido = std::move(b);  (void)movido; });
    fmt::print("  cópia: {} us | move: {} us\n", tc, tm);
    fmt::print("  b.dados.size() após o move = {}\n", b.dados.size());
}
```

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -O2 -Wall -Wextra -I$FMTP/include copiavsmove.cpp -o /tmp/cvm -L$FMTP/lib -lfmtd
/tmp/cvm
```

Saída real (nesta máquina; os tempos variam, a proporção não):

```
  CÓPIA: 1000000 ints copiados
  MOVE : 0 ints copiados (ponteiro trocado)
  cópia: 1207 us | move: 187 us
  b.dados.size() após o move = 0
```

É esta a razão de o move existir: **a cópia tocou 4 MB de memória; o move trocou três
ponteiros.** (Os ~187 µs do move não são o move em si — é a liberação dos mesmos 4 MB quando
`movido` morre no fim da lambda. O trabalho de transferência propriamente dito é constante.)

E repare na última linha: depois do move, `b` ficou com 0 elementos. Isso nos leva ao termo
mais importante da lista abaixo, *válido porém não especificado*.

### O vocabulário, termo a termo

| Termo | Definição |
|---|---|
| **lvalue** | Expressão com **identidade**: tem nome ou endereço e sobrevive à expressão (`x`, `*p`, `arr[0]`). O nome vem de "*left* value", o que podia ficar à esquerda do `=`. |
| **prvalue** (*pure rvalue*) | Expressão **sem** identidade que produz um valor novo: `42`, `x + 1`, `T{}`, uma função que devolve `T` por valor. Não tem endereço; `&42` não compila. |
| **xvalue** (*eXpiring value*) | Expressão que **tem** identidade **e** está expirando, ou seja, pode ser saqueada: `std::move(x)`, uma função que devolve `T&&`. |
| **glvalue** (*generalized lvalue*) | lvalue ∪ xvalue — tudo que tem identidade. |
| **rvalue** | xvalue ∪ prvalue — tudo que pode ser saqueado. |
| **Mover** | Construir/atribuir um objeto **roubando** os recursos internos de outro (ponteiros, buffers), em vez de duplicá-los. |
| **Válido porém não especificado** (*valid but unspecified*) | O estado em que um objeto fica **depois de ser movido**. É legal destruí-lo e é legal atribuir um valor novo a ele; ler o valor antigo também é legal, mas o padrão **não promete qual é**. Para `std::string`/`std::vector` da libstdc++ costuma ficar vazio — isso é observação, não garantia. |
| **Copy elision** (elisão de cópia) | Permissão dada ao compilador de **não chamar** o construtor de cópia/move, construindo o objeto direto no destino — mesmo que esse construtor tivesse efeitos colaterais visíveis (um `fmt::print`, por exemplo). |
| **RVO** (*Return Value Optimization*) | Copy elision no `return` de um **temporário**: `return T{...};`. Desde o C++17 **não é otimização, é regra**: o compilador é obrigado. |
| **NRVO** (*Named RVO*) | Copy elision no `return` de uma **variável local com nome**: `return local;`. Continua sendo permissão, não obrigação — o compilador *pode* aplicar, e o GCC aplica nos casos simples. |
| **Forwarding reference** (referência de repasse) | O nome oficial no padrão para um `T&&` em que `T` é **deduzido** pelo próprio template. O nome antigo, popularizado por Scott Meyers, é *referência universal*; é o oficial que aparece nas mensagens de erro do GCC/Clang. |
| **Colapso de referências** | A regra que transforma `X& &&` em `X&`, etc. Só ocorre onde referências surgem **indiretamente** — dedução de template, `typedef`/`using`, `decltype`. Escrever `int& &` à mão continua sendo ilegal. |
| **`std::move`** | Um `static_cast` para rvalue. Não move nada, não toca em byte nenhum: apenas *marca* a expressão como saqueável. |
| **`std::forward<T>`** | Cast **condicional**: devolve rvalue se `T` foi deduzido de um rvalue, e lvalue se foi deduzido de um lvalue. É o `move` que só age quando é seguro agir. |
| **`std::move_if_noexcept`** | Devolve um rvalue se o move do tipo for `noexcept` **ou** se o tipo não for copiável; caso contrário, devolve um lvalue (forçando a cópia). É assim que `std::vector` decide entre mover e copiar ao realocar. |
| **`std::exchange(obj, novo)`** | Guarda o valor antigo de `obj`, escreve `novo` nele e devolve o antigo. Idioma padrão para escrever um move ctor: `ptr_(std::exchange(o.ptr_, nullptr))`. |
| **Garantias de exceção** | *nothrow*: a operação nunca lança. *Forte*: se lançar, nada mudou (tudo-ou-nada). *Básica*: se lançar, os objetos continuam válidos, mas o estado pode ter mudado. |
| **Sink parameter** (parâmetro-sorvedouro) | Parâmetro recebido **por valor** por uma função que vai **guardar** o argumento; dentro dela, faz-se `std::move` para o destino final. Um único overload atende lvalues (1 cópia) e rvalues (1 move). |
| **Rule of 0/3/5** | Convenções sobre quais das operações especiais declarar. Detalhadas no Demo 2. |

---

## Por que estudar isto?

Porque os erros dessa área são **silenciosos**. Nenhum deles gera warning por padrão:

| Erro | Consequência |
|---|---|
| Declarar `~T() {}` vazio | Suprime os moves — todo "move" vira cópia |
| Esquecer `noexcept` no move ctor | `std::vector` passa a copiar ao realocar |
| `return std::move(x)` | Desliga o NRVO — de 0 operações para 1 |
| `std::move` onde cabia `std::forward` | Rouba o objeto do chamador sem avisar |
| Repassar `T&&` sem `forward` | Vira lvalue no caminho, o move se perde |

O programa roda com contadores instrumentados: cada afirmação acima é **medida**, não
apenas alegada.

---

## Estrutura de arquivos

```
src/value_semantics/
├── main.cpp                      ← ponto de entrada, cor orange; executa os 5 demos
├── demo_value_categories.hpp     ← 1 · lvalue/prvalue/xvalue, decltype, lifetime extension
├── demo_rule_of_five.hpp         ← 2 · Rule of 0/3/5, noexcept, copy-and-swap
├── demo_copy_elision.hpp         ← 3 · RVO garantido, NRVO, o anti-padrão do move
├── demo_perfect_forwarding.hpp   ← 4 · T&&, colapso de referências, std::forward
├── demo_move_only.hpp            ← 5 · tipos move-only, ref-qualifiers, sink parameters
├── meson.build                   ← alvo `value_semantics`, instalado em dist/bin
└── README.md                     ← este arquivo
```

---

## Demo 1: Categorias de Valor

Toda expressão tem um tipo **e** uma categoria. A categoria responde a duas perguntas
independentes: *tem identidade?* e *pode ser movida?*

```
                  expressão
                  ┌───┴───┐
              glvalue    rvalue
              ┌──┴──┐    ┌──┴──┐
          lvalue   xvalue    prvalue
```

| Categoria | Identidade | Movível | Exemplo |
|---|---|---|---|
| lvalue | sim | não | `x`, `*p`, `arr[0]` |
| xvalue | sim | sim | `std::move(x)` |
| prvalue | não | sim | `42`, `f()`, `T{}` |

(É a mesma tabela de [`../syntax/`](../syntax/), seção 7 — as duas não podem divergir.)

### `std::move` é um cast

Comece por aqui, porque é o que desfaz a confusão seguinte:

```cpp
template <typename T>
constexpr std::remove_reference_t<T>&& meu_move(T&& t) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(t);
}
```

Nenhum byte é copiado. O move só acontece quando alguém **constrói ou atribui** a partir do
rvalue resultante — no demo `1.3`:

```
  após meu_move(s), s ainda é: 'conteúdo intacto'
  após 'std::string destino = std::move(s)':
    destino = 'conteúdo intacto'
    s       = ''  ← válido porém não especificado
```

### A regra que mais confunde

```cpp
void recebe(std::string&& s) {
    alvo(s);              // → overload de LVALUE!
    alvo(std::move(s));   // → overload de rvalue
}
```

`s` foi **declarado** como rvalue reference, mas a expressão `s` **tem nome** — e tudo que
tem nome é lvalue. Por isso repassar um `T&&` sem `std::move`/`std::forward` faz cópia.

A regra de bolso: *o tipo da declaração diz o que a variável aceita; a categoria da expressão
diz o que ela é quando você a usa.*

### `decltype` preserva a categoria

```cpp
int x = 10;
decltype(x)             // int    — nome puro usa o tipo declarado
decltype((x))           // int&   — os parênteses fazem virar expressão lvalue
decltype(std::move(x))  // int&&  — xvalue
```

### Lifetime extension — o temporário que não morre

Um temporário morre no fim da expressão completa (o `;`). **Exceto** se você o ligar a uma
referência `const&` ou `&&`: aí o tempo de vida dele é estendido ao da referência.

```cpp
const Ruidoso& r = faz_ruidoso();   // o temporário vive até o fim DESTE escopo
fmt::print("{}", r.tag);            // ✓ legal
```

Saída real do demo `1.6` — repare que o destrutor só roda ao sair do bloco:

```
  const Ruidoso& r = faz_ruidoso();
    Ruidoso('temporário') construído
    ...ainda vivo aqui: tag = 'temporário'
  saindo do escopo:
    Ruidoso('temporário') destruído
```

⚠ **A extensão não é transitiva** — estes casos devolvem referência pendurada (*dangling*):

```cpp
const std::string& s = faz_objeto().membro;  // ✗ o objeto morre; só o MEMBRO seria estendido
                                             //   se o objeto fosse o próprio temporário ligado
const std::string& p(const std::string& x) { return x; }
const std::string& r = p(std::string("oi")); // ✗ passou por um parâmetro: nada é estendido
```

A regra prática: a extensão só vale para o temporário ligado **diretamente**, e só até o fim
do escopo da referência. Atravessar uma função **cancela** a extensão. Em caso de dúvida,
guarde por **valor** — com RVO, isso não custa nada (Demo 3).

---

## Demo 2: Rule of 0 / 3 / 5

Um tipo tem 6 operações que o compilador pode gerar: destrutor, ctor/atribuição de cópia,
ctor/atribuição de move (+ ctor padrão).

### O que você está deixando de escrever

Sem contraste, "Rule of 0" não significa nada. Esta é a classe que **gerencia recurso bruto** —
um `new[]` — e por isso precisa das cinco operações à mão. É o `Buffer` de
[`demo_rule_of_five.hpp`](demo_rule_of_five.hpp), abreviado: o fonte escreve os laços de cópia
à mão e anula a origem membro a membro; aqui usamos `std::copy_n` e `std::exchange`, que são a
forma idiomática de dizer a mesma coisa.

```cpp
class Buffer {
public:
    explicit Buffer(std::size_t n) : tamanho_(n), dados_(new int[n]{}) {}

    ~Buffer() { delete[] dados_; }                                    // 1

    Buffer(const Buffer& o)                                           // 2  deep copy
        : tamanho_(o.tamanho_), dados_(new int[o.tamanho_]) {
        std::copy_n(o.dados_, tamanho_, dados_);
    }

    Buffer& operator=(const Buffer& o) {                              // 3
        if (this != &o) {                        // auto-atribuição
            int* novo = new int[o.tamanho_];     // aloca ANTES de destruir o antigo
            std::copy_n(o.dados_, o.tamanho_, novo);
            delete[] dados_;
            dados_ = novo;  tamanho_ = o.tamanho_;
        }
        return *this;
    }

    Buffer(Buffer&& o) noexcept                                       // 4  rouba o ponteiro
        : tamanho_(std::exchange(o.tamanho_, 0)),
          dados_(std::exchange(o.dados_, nullptr)) {}

    Buffer& operator=(Buffer&& o) noexcept {                          // 5
        if (this != &o) {
            delete[] dados_;
            dados_   = std::exchange(o.dados_, nullptr);
            tamanho_ = std::exchange(o.tamanho_, 0);
        }
        return *this;
    }

private:
    std::size_t tamanho_ = 0;
    int*        dados_   = nullptr;
};
```

Trinta linhas, quatro oportunidades de errar (esquecer a auto-atribuição, esquecer o
`noexcept`, não anular a origem, trocar `delete` por `delete[]`). Agora a **mesma classe**,
com o recurso delegado a quem já sabe geri-lo:

```cpp
class Buffer {
public:
    explicit Buffer(std::size_t n) : dados_(n) {}
private:
    std::vector<int> dados_;
};
```

Zero operações declaradas — e o compilador gera as cinco, corretas, incluindo o move
`noexcept`. **Isto é a Rule of 0.** As Rules 3 e 5 existem para quem escreve o tipo que
gerencia o recurso bruto; isso deve ser raro, e normalmente já foi feito por
`std::vector`, `std::string` ou `std::unique_ptr`.

Contagem real do demo `2.1`, com o `Buffer` instrumentado — 4 construções, 4 destruições,
sem leak e sem double free:

```
  Buffer a(100)                ctor=1 cópia=0/0 move=0/0 dtor=0
  Buffer b = a                 ctor=1 cópia=1/0 move=0/0 dtor=0
  Buffer c = std::move(a)      ctor=1 cópia=1/0 move=1/0 dtor=0
  b = std::move(c)             ctor=1 cópia=1/0 move=1/1 dtor=0
  fim do escopo                ctor=1 cópia=1/0 move=1/1 dtor=3
```

### Um destrutor vazio custa caro

```cpp
struct ComDestrutor {
    std::vector<int> dados;
    ~ComDestrutor() {}      // ← isto sozinho suprime os DOIS moves
};
```

O código continua compilando e rodando. Só fica mais lento. Saída real do demo `2.2`:

```
  ComDestrutor  — move real disponível? NÃO — cai silenciosamente na cópia
  SemDestrutor  — move real disponível? sim
```

> `std::is_move_constructible_v` seria `true` nos dois casos, porque `const T&` também aceita
> rvalue. O discriminador honesto é `std::is_nothrow_move_constructible_v`.

### `noexcept` decide se o `vector` move ou copia

`std::vector` precisa da garantia **forte** ao realocar: se algo lançar no meio da mudança
para o buffer novo, o vetor original tem de continuar íntegro. Mover destrói os elementos de
origem — se o move lançasse na metade do caminho, não haveria como voltar atrás. Copiar, sim,
é reversível: basta jogar fora o buffer novo.

Por isso o `vector` consulta `std::move_if_noexcept`, que devolve um **rvalue** se o move do
tipo for `noexcept` (ou se o tipo nem for copiável) e um **lvalue** caso contrário — forçando
a cópia.

```
  MoveNoexcept    → 7 moves, 0 cópias
  MovePodeLancar  → 0 moves, 7 cópias
```

A única diferença entre os dois tipos é a palavra `noexcept` no construtor de move.

### copy-and-swap

```cpp
Cofre& operator=(Cofre outro) noexcept {   // recebe POR VALOR
    trocar(*this, outro);
    return *this;
}
```

- chamada com lvalue → parâmetro construído por **cópia**
- chamada com rvalue → parâmetro construído por **move**

Um corpo para os dois casos, garantia forte de exceção (a cópia acontece **antes** de qualquer
mudança no `*this`; se ela lançar, nada foi tocado) e auto-atribuição correta de graça.

> ⚠ Declare **apenas** a versão por valor. Ter `operator=(T)` e `operator=(T&&)` ao mesmo
> tempo é ambíguo para argumentos rvalue e não compila.

---

## Demo 3: Copy Elision

### O que é copy elision

É a permissão — em alguns casos, a **obrigação** — de o compilador **não chamar** o construtor
de cópia ou de move, construindo o objeto diretamente no destino final. É a única otimização
do C++ autorizada a eliminar efeitos colaterais visíveis: se o seu construtor de cópia imprime
algo, essa impressão simplesmente não acontece.

### RVO é regra, não otimização

Desde o C++17, retornar um **prvalue** não cria temporário — o objeto é construído diretamente
no destino. A prova é um tipo que **não pode** ser copiado nem movido:

```cpp
struct Imovel {
    Imovel(const Imovel&) = delete;
    Imovel(Imovel&&)      = delete;
};

Imovel faz() { return Imovel(42); }   // compila
Imovel i = faz();                     // compila
```

Se houvesse cópia ou move em algum ponto, isso nem compilaria. Consequência prática:
**devolver por valor é o padrão**; não invente parâmetros de saída para "evitar a cópia".

### NRVO — o caso da variável com nome

`return local;` (variável nomeada) é permissão, não obrigação: o compilador *pode* construir
`local` já no endereço do destino. O GCC faz isso nos casos simples — demo `3.3` mede
**0 cópias, 0 moves**.

### O anti-padrão

```cpp
Rastreado ruim() {
    Rastreado local("x");
    return std::move(local);   // ✗ mata o NRVO
}
```

| Versão | Cópias | Moves |
|---|---|---|
| `return local;` (NRVO) | 0 | 0 |
| `return std::move(local);` | 0 | **1** |

A intenção é boa, o efeito é o oposto: `std::move(local)` vira xvalue, e a elisão só se
aplica ao *nome* da variável. O GCC avisa com `-Wpessimizing-move`.

### Quando a elisão não se aplica

```cpp
Rastreado f(bool c) {
    Rastreado a("A"), b("B");
    if (c) return a;    // dois candidatos → NRVO inviável
    return b;           // → move implícito (não cópia)
}
```

Retornar uma variável local trata a expressão como rvalue, então o fallback é o **move** —
nunca a cópia. Você não precisa escrever `std::move` para obter isso.

---

## Demo 4: Perfect Forwarding

Repassar argumentos preservando **tipo**, **constância** e **categoria** exige duas peças que
só funcionam juntas:

```cpp
template <typename T>
void repassa(T&& arg) {          // 1. forwarding reference
    alvo(std::forward<T>(arg));  // 2. forward condicional
}
```

Aqui `T&&` é uma **forwarding reference** — o nome oficial no padrão para o `T&&` com `T`
deduzido. O nome antigo e ainda muito usado é *referência universal*; é a forma oficial que
aparece nas mensagens de erro do GCC e do Clang.

### Colapso de referências

Quando `T` é deduzido em `T&&`:

| Argumento | `T` deduzido | Parâmetro final |
|---|---|---|
| lvalue de tipo `X` | `X&` | `X& &&` → **`X&`** |
| rvalue de tipo `X` | `X` | **`X&&`** |

Tabela geral — só `&& &&` sobrevive como rvalue:

```
X&  &  → X&        X&  && → X&
X&& &  → X&        X&& && → X&&
```

⚠ O colapso só acontece onde a referência a referência surge **indiretamente**: dedução de
template, `typedef`/`using`, `decltype`, parâmetro de template. Escrever `int& &` à mão
continua sendo erro de sintaxe.

### Nem todo `T&&` é forwarding reference

```cpp
void f(std::string&&);                          // rvalue ref pura
template<class T> void f(std::vector<T>&&);     // rvalue ref pura — T não está na posição
template<class T> void f(T&&);                  // ✓ forwarding reference
```

A exigência é que o parâmetro seja **exatamente** `T&&` com `T` deduzido nessa mesma função.
`const T&&` também não conta.

### `move` é incondicional, `forward` é condicional

```cpp
template <typename T> Registro errado(T&& n) { return {std::move(n)};        }  // ✗
template <typename T> Registro certo (T&& n) { return {std::forward<T>(n)};  }  // ✓
```

```
    após cria_errado(meu):  meu = ''  ← esvaziado sem avisar!
    após cria_certo(meu):   meu = 'ainda preciso disto'  ← intacto
```

**Regra:** `std::move` em parâmetro de tipo concreto; `std::forward<T>` em parâmetro deduzido.

### O objeto movido: "válido porém não especificado"

Depois de `std::string destino = std::move(origem);`, o que sobrou em `origem`?

- **legal**: destruí-la, atribuir um valor novo, chamar qualquer método sem pré-condição
  (`clear()`, `size()`, `empty()`);
- **legal, mas sem promessa**: ler o valor. O padrão diz apenas *válido porém não
  especificado*. A libstdc++ deixa `std::string` e `std::vector` vazios, e o demo mostra
  `''` — isso é **observação da implementação**, não garantia da linguagem;
- **UB**: qualquer método com pré-condição sobre o conteúdo, como `front()` num container
  possivelmente vazio.

Regra prática: depois de mover, ou você **reatribui** ou **para de usar** o objeto.

### `auto&&` e o range-for

`auto&&` também é forwarding reference: liga-se a qualquer coisa e **nunca copia**. É por isso
que o range-for genérico se escreve assim:

```cpp
for (auto&& x : v) { ... }   // funciona com lvalue, temporário, proxy (vector<bool>!)
for (const auto& x : v) { }  // quando você só lê — a escolha mais comum
for (auto x : v) { }         // quando você quer mesmo uma cópia
```

O `vector<bool>` é o caso que obriga: seu `operator[]` devolve um **proxy** por valor (um
prvalue), e `auto&` não se ligaria a ele.

⚠ Cuidado com temporários na **expressão** do range-for. O objeto ligado diretamente ao
`__range` interno sempre teve o tempo de vida estendido, mas os temporários intermediários não:

```cpp
for (auto& x : faz_objeto())          // ✓ o temporário devolvido é estendido
for (auto& x : faz_objeto().itens())  // ✗ até o C++20: faz_objeto() morre antes do laço
```

Só o **C++23** (P2718R0) passou a estender também esses temporários intermediários. Compilando
em C++20, como este repositório faz, a segunda linha é *dangling* silencioso — guarde o objeto
numa variável antes do laço.

---

## Demo 5: Move-only, Ref-Qualifiers e Sink Parameters

### Tipos move-only

Alguns recursos não podem ser duplicados — descritor de arquivo, lock, socket, thread.
Copiar não é caro, é **errado**:

```cpp
class Conexao {
    Conexao(const Conexao&)            = delete;   // torna o erro incompilável
    Conexao& operator=(const Conexao&) = delete;
    Conexao(Conexao&&) noexcept;                   // posse transferível
    Conexao& operator=(Conexao&&) noexcept;
};
```

O exemplo canônico da biblioteca padrão é `std::unique_ptr` — ver
[`../smart-pointers/`](../smart-pointers/). Saída real do demo `5.1`, mostrando que o recurso
é fechado **uma única vez**:

```
      abrindo conexão com 'db.exemplo.com'
      a.aberta() = false  ← deixou de ser dona
      b.aberta() = true  ← assumiu o recurso
      fechando conexão com 'db.exemplo.com'
```

### Ref-qualifiers

Um método pode perguntar se `*this` é descartável:

```cpp
const std::string& texto() const&  { return texto_; }             // lvalue → referência
std::string        texto() &&      { return std::move(texto_); }  // rvalue → rouba
```

Também servem para **proibir**: `T& operator[](int) && = delete;` impede devolver referência
para dentro de um temporário que já morreu.

### Sink parameter

Quando a função vai **guardar** uma cópia:

```cpp
void adicionar(std::string nome) {          // por valor
    nomes_.push_back(std::move(nome));      // + move (obrigatório!)
}
```

- lvalue → 1 cópia (inevitável: o chamador quer guardar)
- rvalue → 1 move (barato)

⚠ Esquecer o `std::move` de dentro anula o ganho: `nome` tem nome, logo é lvalue, logo
`push_back` copiaria de novo.

Um overload cobre os dois casos. A alternativa (`const T&` **e** `T&&`) explode em 2ⁿ
overloads para n parâmetros.

---

## Regras de ouro

| Intenção | Assinatura |
|---|---|
| Só ler o argumento | `const T&` |
| Ler e modificar o do chamador | `T&` |
| Guardar uma cópia | `T` por valor + `std::move` dentro |
| Repassar em template | `T&&` + `std::forward<T>` |
| Tipo pequeno e trivial | `T` por valor |

| Situação | Faça |
|---|---|
| Tipo composto de `vector`/`string`/`unique_ptr` | **Rule of 0** — não declare nada |
| Precisou declarar o destrutor | Declare as outras 4 (ou `= default`) |
| Escreveu um move ctor | Marque `noexcept` e use `std::exchange` |
| Vai retornar uma variável local | `return local;` — nunca `return std::move(local)` |
| Parâmetro deduzido `T&&` | `std::forward<T>`, nunca `std::move` |
| Recurso com dono único | Delete a cópia, mantenha o move |
| Depois de mover um objeto | Reatribua ou pare de usá-lo |

Os avisos do compilador que pegam exatamente estes erros (nenhum vem ligado sem `-Wall`):

| Flag | Pega |
|---|---|
| `-Wpessimizing-move` | `return std::move(local)` — o anti-padrão do Demo 3 |
| `-Wredundant-move` | `std::move` onde o move já aconteceria sozinho |
| `-Wreturn-std-move` (Clang) | o caso inverso: `return local;` onde o move **precisava** ser explícito |
| `-Wself-move` | `x = std::move(x)` |

No GCC 13.3, `-Wall` liga `-Wpessimizing-move` e `-Wself-move`; `-Wextra` liga
`-Wredundant-move`. `-Wreturn-std-move` só existe no Clang. Ou seja:

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic ...   # os três avisos do GCC, ligados
```

---

## Como validar

### 1. Rodar o programa instalado

```bash
make build
./dist/bin/value_semantics
```

Os 5 blocos, com o cabeçalho em laranja, e trechos **reais** do que deve aparecer em cada um:

```
╔══════════════════════════════════════════════════════╗
║  1 · Categorias de valor — lvalue, prvalue, xvalue   ║
╚══════════════════════════════════════════════════════╝
  após 'std::string destino = std::move(s)':
    s       = ''  ← válido porém não especificado
  decltype((x))          = int&   (parênteses → expressão lvalue!)

║  2 · Rule of 0/3/5 — as operações especiais          ║
  fim do escopo                ctor=1 cópia=1/0 move=1/1 dtor=3
  ComDestrutor  — move real disponível? NÃO — cai silenciosamente na cópia
  MoveNoexcept    → 7 moves, 0 cópias
  MovePodeLancar  → 0 moves, 7 cópias

║  3 · Copy elision — RVO, NRVO e o move ausente       ║
    → RVO: 0 cópia(s), 0 move(s)
    → NRVO: 0 cópia(s), 0 move(s)
    → return std::move(local): 0 cópia(s), 1 move(s)

║  4 · Perfect forwarding — T&& + std::forward         ║
      ↑ os dois foram parar no overload de lvalue — o move se perdeu
    após cria_errado(meu):  meu = ''  ← esvaziado sem avisar!
    após cria_certo(meu):   meu = 'ainda preciso disto'  ← intacto

║  5 · Move-only, ref-qualifiers e sink parameters     ║
      saindo do escopo — só UM fechamento deve aparecer:
      após adicionar(rvalue): nome = ''  ← movido
```

A última linha do programa, em verde:

```
✓ Todos os demos de semântica de valor concluídos!
```

O número que mais importa conferir é o par `7 moves, 0 cópias` × `0 moves, 7 cópias`: se os
dois ficarem iguais, o `noexcept` do move deixou de ter efeito.

### 2. Compilar avulso, com todos os avisos

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/value_semantics/main.cpp -o /tmp/vs -L$FMTP/lib -lfmtd && /tmp/vs
```

### 3. Rodar sob sanitizers

O subprojeto mexe com tempo de vida (lifetime extension, objetos movidos, `new[]` cru no
`Buffer`), então vale rodar sob ASan+UBSan ao menos uma vez:

```bash
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/value_semantics/main.cpp -o /tmp/vssan -L$FMTP/lib -lfmtd && /tmp/vssan
```

**Estado atual (verificado):** compila **sem nenhum aviso** com `-Wall -Wextra -Wpedantic` e
roda com **0 achados** sob ASan+UBSan. Um único `#pragma GCC diagnostic` local, em
`demo_copy_elision.hpp`, silencia `-Wpessimizing-move` exatamente no ponto em que o
anti-padrão **é a lição** — o comentário no fonte explica por quê.

---

## Referências

Neste repositório:

- [`src/syntax/`](../syntax/) — a sintaxe de tudo isto; categorias de valor na seção 7
- [`src/smart-pointers/`](../smart-pointers/) — `unique_ptr`, o tipo move-only canônico
- [`src/oop/`](../oop/) — *slicing*: o caso em que a cópia acontece e destrói o objeto
- [`src/performance/`](../performance/) — o custo medido de copiar em vez de mover
- [`src/layout/`](../layout/) — por que um objeto pequeno é barato de copiar
- [`src/stl/`](../stl/) — os containers que aplicam tudo isto por você
- [`src/errors/`](../errors/) — as garantias de exceção em profundidade

Fontes canônicas:

- **cppreference** — [Value categories](https://en.cppreference.com/w/cpp/language/value_category),
  [Copy elision](https://en.cppreference.com/w/cpp/language/copy_elision),
  [`std::move`](https://en.cppreference.com/w/cpp/utility/move),
  [`std::forward`](https://en.cppreference.com/w/cpp/utility/forward),
  [`std::move_if_noexcept`](https://en.cppreference.com/w/cpp/utility/move_if_noexcept),
  [Move constructors](https://en.cppreference.com/w/cpp/language/move_constructor),
  [Rule of three/five/zero](https://en.cppreference.com/w/cpp/language/rule_of_three)
- **C++ Core Guidelines** —
  [F.18](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#f18-for-will-move-from-parameters-pass-by-x-and-stdmove-the-parameter)
  (parâmetro consumido), F.19 (repasse), C.20 (Rule of 0), C.21 (Rule of 5), C.66 (move
  `noexcept`)
- *Effective Modern C++*, Scott Meyers — itens 23 a 30 (move, forwarding, colapso de
  referências, os casos em que o perfect forwarding falha)
- **Padrão ISO** — `[basic.lval]` (categorias), `[class.copy.elision]` (elisão),
  `[dcl.init.ref]` (extensão de tempo de vida)
