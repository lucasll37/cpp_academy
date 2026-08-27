# concepts — dizer ao compilador o que um template exige

## O que é este projeto?

Um template em C++ aceita **qualquer** tipo. `concept` é o recurso do C++20 que permite
escrever, na própria assinatura, **quais tipos são aceitáveis** — e obter uma mensagem de
erro que aponta o requisito violado em vez de despejar um muro de texto.

Este subprojeto vai do zero (o que é um `concept`, em uma frase) até subsumption, NTTP e
policy-based design, com todos os exemplos rodando.

```bash
make build          # configura + compila + instala em dist/
./dist/bin/concepts
```

Saída final esperada: `✓ Todos os demos de concepts concluidos!`

> **Requer C++20.** A flag vem de `default_options: ['cpp_std=c++20']` no
> [`meson.build` da raiz](../../meson.build) — o [`meson.build`](meson.build) deste
> subprojeto **não** define padrão nenhum, ele só declara o executável `concepts`.

---

## Antes de começar

Esta seção define **cada** termo usado no resto do documento. Se você já os conhece, pule
para "Por que estudar isto?".

**Template.** Um molde de código com um "buraco" no lugar de um tipo. O compilador preenche
o buraco quando você usa o molde:

```cpp
template <typename T>   // T é o buraco
T maior(T a, T b) { return a > b ? a : b; }

maior(3, 7);            // o compilador gera uma versão com T = int
maior(1.5, 2.5);        // e outra com T = double
```

**Instanciação.** O ato de preencher o buraco e gerar código de verdade. Acontece durante a
**compilação** — não existe "template" no binário, só as versões instanciadas.

**Compile-time × runtime.** Tudo que este subprojeto discute acontece em *compile-time*: o
compilador decide se o código sequer existe. Nada aqui custa um ciclo de CPU em execução.

**O problema que o template acima tem.** Nada impede `maior(std::vector<int>{}, ...)`. O
molde aceita, tenta instanciar, e o erro só aparece lá dentro, na linha do `a > b` — dentro
do *seu* código, não no ponto onde o chamador errou.

**Type trait.** Uma struct/variável de template que responde uma pergunta sobre um tipo em
compile-time. `std::is_integral_v<int>` é `true`; `std::is_integral_v<double>` é `false`.
Traits estão em `<type_traits>` e existem desde o C++11.

**Predicado.** Uma expressão que resulta em `true` ou `false`. Um `concept` é exatamente
isso: um predicado sobre tipos, avaliado em compile-time.

**SFINAE** (*Substitution Failure Is Not An Error*). A regra antiga, do C++98/11: se, ao
tentar preencher o buraco de um template, a substituição produzir código inválido **na
assinatura**, o compilador não emite erro — ele apenas **descarta** aquele candidato. Era o
único jeito de fazer "esta função só vale para inteiros" antes do C++20.

**`std::enable_if`.** A ferramenta usada para explorar SFINAE. `std::enable_if<cond, T>` tem
um membro `::type` **só quando `cond` é `true`**. Escrever `typename std::enable_if<...>::type`
como tipo de retorno faz a assinatura ficar inválida quando a condição é falsa — e o
candidato some. Funciona, e é ilegível. Há mais sobre isso em [`src/templates/`](../templates/).

**Duck typing.** "Se anda como um pato e grasna como um pato, é um pato": aceitar um tipo
pelas **operações que ele suporta**, não pela classe de que ele herda. Templates sempre
fizeram duck typing; concepts o tornam **verificado e documentado**.

**NTTP** (*Non-Type Template Parameter*). Um parâmetro de template que é um **valor**, não um
tipo: o `5` em `std::array<int, 5>`.

**Contexto não avaliado.** O interior de um `requires { ... }` **nunca é executado**. O
compilador só verifica se aquelas expressões *compilariam*. Você pode escrever
`{ a.destruir_o_universo() }` num concept sem medo: nada roda.

**Sobrecarga (*overload*) e resolução de sobrecarga.** Várias funções com o mesmo nome e
assinaturas diferentes; o compilador escolhe uma. Quando duas candidatas são igualmente boas
em tipo, o desempate pelas *constraints* é o que se chama de **subsumption** (Demo 3).

---

## Por que estudar isto?

Comece pelo problema, não pela sintaxe. Você escreve uma função só para inteiros:

```cpp
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
soma(T a, T b) { return a + b; }
```

Alguém chama `soma(std::string("a"), std::string("b"))`. O que o GCC 13 responde é isto —
**saída real**, não paráfrase:

```
error: no matching function for call to 'soma(std::string, std::string)'
note: candidate: 'template<class T> typename std::enable_if<std::is_integral<_Tp>::value, T>::type soma(T, T)'
note:   template argument deduction/substitution failed:
In substitution of 'template<class T> ... soma(T, T) [with T = std::__cxx11::basic_string<char>]':
error: no type named 'type' in 'struct std::enable_if<false, std::__cxx11::basic_string<char> >'
```

A última linha é a explicação verdadeira, e ela fala de `enable_if`, `false` e `type` — três
coisas que **não têm nada a ver com o problema do usuário**, que é "string não é inteiro".

A mesma função com concept:

```cpp
template <std::integral T>
T soma(T a, T b) { return a + b; }
```

```
error: no matching function for call to 'soma(std::string, std::string)'
note: candidate: 'template<class T>  requires  integral<T> T soma(T, T)'
note: constraints not satisfied
    required for the satisfaction of 'integral<T>' [with T = std::__cxx11::basic_string<char, ...>]
note: the expression 'is_integral_v<_Tp> [with _Tp = std::__cxx11::basic_string<char, ...>]' evaluated to 'false'
```

`constraints not satisfied` + o requisito exato que falhou. É este o ganho — e você pode
reproduzir os dois blocos acima em 30 segundos: veja [Como validar](#como-validar).

Além da mensagem, concepts trazem três coisas que o SFINAE não tinha:

| | SFINAE / `enable_if` | Concepts |
|---|---|---|
| Legibilidade | a condição vive no tipo de retorno | a condição vive onde se lê |
| Reúso | copiar-colar a expressão | dar um nome e usar o nome |
| Escolha entre sobrecargas | manual (`enable_if` mutuamente exclusivos) | automática por subsumption |
| Onde o erro aparece | dentro da expansão | no ponto da chamada, nomeando o requisito |

---

## Estrutura de arquivos

```
src/concepts/
├── main.cpp               ← ponto de entrada, roda os 4 demos em sequência
├── demo_basics.hpp        ← 1 · definir concepts, 4 formas de aplicar, concepts da stdlib
├── demo_requires.hpp      ← 2 · os 4 tipos de requisito dentro de requires{}
├── demo_constraints.hpp   ← 3 · subsumption, classes, lambdas, NTTP, contratos de API
├── demo_real_world.hpp    ← 4 · algoritmos, policies, concepts vs herança
├── meson.build
└── README.md
```

---

## Demo 1: Fundamentos — [`demo_basics.hpp`](demo_basics.hpp)

### O que é um concept, em uma frase

Um concept é um **nome dado a um predicado sobre tipos**. Programa completo, compilável:

```cpp
#include <concepts>
#include <cstdio>

template <typename T>
concept Numerico = std::integral<T> || std::floating_point<T>;

Numerico auto dobro(Numerico auto x) { return x * 2; }

int main() {
    std::printf("%d\n",   dobro(21));    // 42
    std::printf("%.2f\n", dobro(1.57));  // 3.14
    // dobro("texto");                   // ← não compila; veja a mensagem abaixo
}
```

```
42
3.14
```

Descomentando a última linha, o GCC 13 diz exatamente qual metade da disjunção falhou:

```
error: no matching function for call to 'dobro(const char [6])'
note: constraints not satisfied
    required for the satisfaction of 'Numerico<auto:1>' [with auto:1 = const char*]
note: no operand of the disjunction is satisfied
    5 | concept Numerico = std::integral<T> || std::floating_point<T>;
      |                    ~~~~~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~
```

Três elementos aparecem aí, e vale nomeá-los:

- **`concept Nome = expressão;`** — a definição. A expressão precisa ser um `bool` de
  compile-time.
- **`Numerico auto x`** — o uso. `auto` num parâmetro de função já cria um parâmetro de
  template implícito (isso se chama *abbreviated function template*); pôr o nome de um
  concept na frente restringe esse parâmetro.
- **satisfação** — dizer que "`int` satisfaz `Numerico`" é dizer que a expressão dá `true`
  com `T = int`.

### As quatro formas de aplicar um concept

Todas equivalentes; o fonte tem as quatro lado a lado:

```cpp
// 1. no lugar de `typename`
template <Numerico T>          T dobrar_v1(T x) { return x * 2; }

// 2. cláusula requires DEPOIS da assinatura
template <typename T>          T dobrar_v2(T x) requires Numerico<T> { return x * 2; }

// 3. cláusula requires depois da lista de parâmetros do template
template <typename T> requires Numerico<T>
                               T dobrar_v3(T x) { return x * 2; }

// 4. auto restringido (abbreviated function template)
Numerico auto dobrar_v4(Numerico auto x) { return x * 2; }
```

```
dobrar_v1(21)   = 42
dobrar_v2(3.14) = 6.28
dobrar_v3(10)   = 20
dobrar_v4(5.5f) = 11.0
```

Use a forma 1 quando a restrição tem nome; a 2/3 quando a condição é uma expressão avulsa
(`requires (sizeof(T) == 4)`); a 4 quando o parâmetro é curto e o template não precisa se
referir a `T` pelo nome.

### Concepts prontos da biblioteca padrão

```cpp
// Em <concepts>
std::integral<T>            // int, long, short, char, bool...
std::signed_integral<T>
std::unsigned_integral<T>
std::floating_point<T>      // float, double, long double
std::same_as<T, U>          // T é exatamente U
std::derived_from<D, B>     // D herda publicamente de B
std::convertible_to<T, U>
std::equality_comparable<T> // tem == e !=
std::totally_ordered<T>     // tem <, >, <=, >=
std::copyable<T> / std::movable<T>
std::regular<T>             // copyable + default_initializable + equality_comparable
std::invocable<F, Args...>  // F pode ser chamado com Args...

// Em <iterator>
std::input_iterator<I>
std::forward_iterator<I>
std::random_access_iterator<I>

// Em <ranges>
std::ranges::range<R>
std::ranges::input_range<R>
std::ranges::range_value_t<R>   // (isto é um alias de tipo, não um concept)
```

> ⚠ **Não existe `std::arithmetic`.** É a confusão mais comum: `<type_traits>` tem o *trait*
> `std::is_arithmetic_v<T>`, mas `<concepts>` **não** tem um concept `std::arithmetic`.
> `template <std::arithmetic T>` responde `error: 'std::arithmetic' has not been declared`.
> Componha você mesmo — é o que [`demo_basics.hpp`](demo_basics.hpp) faz:
> ```cpp
> template <typename T>
> concept Numerico = std::integral<T> || std::floating_point<T>;
> ```

### Compondo concepts

`&&`, `||` e `!` funcionam entre concepts e produzem outro predicado:

```cpp
template <typename T> concept Inteiro32 = std::integral<T> && (sizeof(T) == 4);
template <typename T> concept NaoConst  = !std::is_const_v<T>;
```

```
Inteiro32<int>:      true
Inteiro32<short>:    false
Inteiro32<long>:     false     ← neste x86-64 Linux, long tem 8 bytes
NaoConst<int>:       true
NaoConst<const int>: false
```

---

## Demo 2: `requires` — [`demo_requires.hpp`](demo_requires.hpp)

Até aqui as restrições eram sobre *o que o tipo é*. A **requires-expression** restringe *o
que dá para fazer com ele*: você lista operações, e o compilador verifica se cada uma
compilaria.

> ⚠ Nada ali dentro é executado. É [contexto não avaliado](#antes-de-começar): só a
> validade das expressões importa.

### `requires` cláusula × `requires` expressão

São **duas coisas diferentes** com a mesma palavra, e é daí que vem a confusão do
`requires requires`:

| Forma | O que ela recebe | O que ela é |
|---|---|---|
| **cláusula** `requires C<T>` | uma expressão booleana de compile-time | parte da assinatura |
| **expressão** `requires(T t) { t.size(); }` | uma lista de requisitos | **produz** um `bool` |

```cpp
template <typename T> requires std::integral<T>   // ← CLÁUSULA: espera um bool
void f(T);

template <typename T>
concept TemSize = requires(T t) { t.size(); };    // ← EXPRESSÃO: produz um bool
```

Quando você quer uma cláusula cujo bool venha de uma expressão escrita ali mesmo, sem dar
nome a ela, as duas se encostam:

```cpp
template <typename T>
    requires requires(T t) { t.size(); }   // cláusula + expressão = "requires requires"
void g(T);
```

Não é erro de digitação, é a leitura literal: *"esta função exige (`requires`) que valha o
predicado `requires(T t){ t.size(); }`"*. Na prática, **dê um nome ao predicado** e escreva
`requires TemSize<T>` — é o que os fontes deste subprojeto fazem.

### Os quatro tipos de requisito

**1. Simple requirement** — a expressão só precisa compilar; o tipo do resultado é ignorado.

```cpp
template <typename T>
concept Somavel = requires(T a, T b) {
    a + b;        // precisa compilar
    a - b;
    a += b;
};
```

**2. Type requirement** — o tipo aninhado precisa existir.

```cpp
template <typename T>
concept Container = requires {
    typename T::value_type;
    typename T::iterator;
    typename T::const_iterator;
} && requires(T c) {
    c.begin(); c.end(); c.size();
};
```

```
Container<std::vector<int>>: true
Container<std::string>:      true
Container<int>:              false
```

**3. Compound requirement** — verifica a expressão **e** o tipo do resultado, e opcionalmente
que ela seja `noexcept`:

```cpp
{ expressao } -> Concept;             // o resultado satisfaz Concept
{ expressao } noexcept;               // a expressão não lança
{ expressao } noexcept -> Concept;    // as duas coisas
```

```cpp
template <typename T>
concept Serializavel = requires(const T& obj) {
    { obj.to_string() } -> std::same_as<std::string>;
};
```

> ⚠ **Os requisitos de um `requires{}` são conjuntivos (E), nunca alternativos (OU).**
> Este concept exige que `a + b` seja **ao mesmo tempo** exatamente `T` **e** conversível
> para `int`:
> ```cpp
> template <typename T>
> concept Somavel = requires(T a, T b) {
>     { a + b } -> std::same_as<T>;           // E
>     { a + b } -> std::convertible_to<int>;  // E  ← não é "ou"
> };
> ```
> Medido: `Somavel<int>` = `true`, `Somavel<double>` = `true`,
> `Somavel<std::string>` = **`false`** — `string + string` é `string`, que satisfaz o
> primeiro requisito e reprova no segundo. Para alternativas, use `||` **entre concepts**,
> fora do `requires{}`.

**4. Nested requirement** — um `requires` *dentro* do bloco, para exigir que um predicado
seja **verdadeiro** (e não apenas que ele compile):

```cpp
template <typename T>
concept NumericoPreciso = requires {
    requires std::floating_point<T>;         // precisa ser VERDADE
    requires (sizeof(T) >= sizeof(float));
};
```

```
NumericoPreciso<float>:  true
NumericoPreciso<double>: true
NumericoPreciso<int>:    false
```

> ⚠ **A armadilha nº 1 dos concepts.** Esquecer o `requires` interno transforma o requisito
> num *simple requirement*: o compilador passa a checar apenas se o texto
> `std::integral<T>` **compila** — e ele sempre compila, porque é só uma variável booleana.
> O concept vira `true` para tudo:
> ```cpp
> template <typename T> concept Errado  = requires { std::integral<T>; };          // ✗
> template <typename T> concept Correto = requires { requires std::integral<T>; }; // ✓
> ```
> ```
> Errado<std::string>  = 1     ← passou! e não devia
> Correto<std::string> = 0
> ```
> O GCC avisa: `warning: testing if a concept-id is a valid expression; add 'requires' to
> check satisfaction [-Wmissing-requires]`. Compile com `-Wall` e leia os avisos.

### `requires` solto, como `bool`

Você não precisa nomear um concept para usar a expressão:

```cpp
template <typename T>
void inspecionar(const T&) {
    constexpr bool tem_size  = requires(T x) { x.size(); };
    constexpr bool tem_begin = requires(T x) { x.begin(); };
    if constexpr (tem_size) { /* ... */ }
}
```

Serve dentro de `static_assert` e `if constexpr`. Se a mesma condição aparece duas vezes,
promova-a a `concept`.

---

## Demo 3: Constraints avançadas — [`demo_constraints.hpp`](demo_constraints.hpp)

### Subsumption — o compilador prefere a restrição mais forte

Quando duas sobrecargas são viáveis, vence a de constraint **mais específica**:

```cpp
template <typename T>
concept Numerico = std::integral<T> || std::floating_point<T>;

template <Numerico T>              std::string categoria(T) { return "numerico generico"; }
template <std::integral T>         std::string categoria(T) { return "inteiro (mais especializado)"; }
template <std::floating_point T>   std::string categoria(T) { return "ponto-flutuante (mais especializado)"; }
```

```
categoria(42)    = inteiro (mais especializado)
categoria(3.14)  = ponto-flutuante (mais especializado)
categoria(2.0f)  = ponto-flutuante (mais especializado)
```

**A regra de verdade**, e não só "é mais específico": o compilador **normaliza** cada
constraint até um conjunto de *constraints atômicas* (as folhas: expressões que ele não sabe
decompor mais) e diz que **A subsume B quando o conjunto de A implica o de B**. Duas
constraints atômicas só são consideradas a mesma se vierem **literalmente da mesma expressão
no mesmo lugar do código**.

> ⚠ **É por isso que subsumption só funciona com concepts *nomeados* e compostos.** Se você
> repetir um type trait em duas constraints, elas viram duas atômicas **diferentes**, nenhuma
> subsume a outra, e a chamada fica ambígua:
> ```cpp
> template <typename T> concept NumericoA = std::is_arithmetic_v<T>;
> template <typename T> concept NumericoB = std::is_arithmetic_v<T> && std::is_integral_v<T>;
> template <NumericoA T> void p(T);
> template <NumericoB T> void p(T);
> p(42);
> ```
> ```
> error: call of overloaded 'p(int)' is ambiguous
> ```
> Escreva `NumericoB = NumericoA<T> && std::integral<T>` — em termos de `NumericoA` — e o
> compilador passa a enxergar a implicação. O cabeçalho do
> [`demo_constraints.hpp`](demo_constraints.hpp) resume: *"para o compilador detectar isso,
> A deve ser definido EM TERMOS de B"*.

### Concepts em class templates — métodos condicionais

`requires` num método individual faz o método **existir apenas** para os tipos que
satisfazem a condição. Não é um `static_assert`: o método simplesmente não entra na
sobrecarga.

```cpp
template <typename T>
class Caixa {
public:
    explicit Caixa(T val) : valor_(val) {}
    const T& valor() const { return valor_; }

    T dobrar() const requires Numerico<T>              { return valor_ * 2; }
    bool menor_que(const Caixa& o) const requires std::totally_ordered<T> { return valor_ < o.valor_; }
    std::size_t tamanho() const requires std::same_as<T, std::string>     { return valor_.size(); }
private:
    T valor_;
};
```

```
Caixa<int>(21).dobrar()      = 42
Caixa<double>(3.14).dobrar() = 6.28
Caixa<string>.tamanho()      = 7
```

`Caixa<std::string>` **compila** — só não tem `dobrar()`. É a alternativa moderna e legível à
especialização parcial usada para o mesmo fim.

### Concepts em lambdas

`auto` num parâmetro de lambda já era um template desde o C++14; o C++20 deixa restringi-lo:

```cpp
auto quadrado_inteiro = [](std::integral auto x)       { return x * x; };
auto somar_numericos  = [](Numerico auto a, Numerico auto b) { return a + b; };
```

```
quadrado_inteiro(7)      = 49
quadrado_inteiro(10L)    = 100
somar_numericos(3, 4)    = 7
somar_numericos(1.5,2.5) = 4.0
```

Útil quando a lambda vai para um algoritmo genérico e você quer que o erro apareça na
chamada, não três níveis abaixo.

### Restringindo NTTP (parâmetros de template que são valores)

A cláusula `requires` também aceita condições sobre **valores** de template:

```cpp
template <typename T, std::size_t N>
    requires (N > 0)
class ArrayFixo {
    std::array<T, N> dados_;
public:
    constexpr std::size_t tamanho() const { return N; }
    // ...
};

ArrayFixo<int, 5>  ok;
// ArrayFixo<int, 0> ruim;   // error: constraint (N > 0) não satisfeita
```

```
ArrayFixo<5>: [0, 1, 4, 9, 16]
ArrayFixo<3>: [1.1, 2.2, 3.3]
```

Os parênteses em `requires (N > 0)` são obrigatórios: sem eles o `>` é lido como o fim de uma
lista de parâmetros de template.

### Concept como contrato de API

Um concept com dois parâmetros documenta e **verifica** a relação entre dois tipos:

```cpp
template <typename H, typename Event>
concept EventHandler = requires(H handler, const Event& event) {
    { handler.on_event(event) } -> std::same_as<void>;
    { handler.nome() }          -> std::convertible_to<std::string>;
};

template <typename H, typename E> requires EventHandler<H, E>
void disparar_evento(H& handler, const E& event);
```

```
EventHandler<HandlerUI, EventoClick>:   true
EventHandler<HandlerUI, EventoTeclado>: true
EventHandler<int, EventoClick>:         false
```

---

## Demo 4: Casos reais — [`demo_real_world.hpp`](demo_real_world.hpp)

### Algoritmo genérico: um degrau de cada vez

O salto de "concept de 5 linhas" para "algoritmo sobre ranges" costuma derrubar o leitor.
Faça em três etapas.

**Etapa 1 — restringir o elemento.**

```cpp
template <typename T>
concept Estatistico = std::floating_point<T> ||
                      (std::integral<T> && !std::same_as<T, bool>);
```

O `!same_as<T,bool>` está aí porque `bool` é integral e uma média de `bool` não significa
nada.

**Etapa 2 — restringir o contêiner, reusando o concept do elemento.**

```cpp
template <typename R>
concept RangeNumerico = requires(R r) {
    r.begin();
    r.end();
    requires Estatistico<typename R::value_type>;   // ← nested: EXIGE que seja verdade
};
```

**Etapa 3 — o algoritmo, agora com um contrato de uma palavra.**

```cpp
template <RangeNumerico R>
auto media(const R& range) {
    if (range.empty()) return static_cast<double>(0);
    auto soma = std::accumulate(range.begin(), range.end(), static_cast<double>(0));
    return soma / static_cast<double>(range.size());
}
```

(O `static_cast<double>(0)` em vez de `0.0` é proposital: `std::accumulate` deduz o tipo do
acumulador a partir do valor inicial. Passar `0` faria a soma inteira ser feita em `int`, e a
média de `[1, 2]` daria `1`.)

```
vi = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3]
  média:    3.90
  variância:6.10
  mediana:  3.5
vd = [2.5, 3.7, 1.1, 4.8, 2.2]
  média:    2.86
  variância:2.03
  mediana:  2.5
```

A versão da biblioteca padrão troca `r.begin()/r.end()` e `R::value_type` pelos concepts de
[`<ranges>`](../ranges/), que também funcionam para arrays cruos e para tipos sem membros
aninhados:

```cpp
template <std::ranges::input_range Range, typename Pred>
    requires std::predicate<Pred, std::ranges::range_value_t<Range>>
auto filtrar(const Range& r, Pred pred);
```

Lendo por partes: `input_range` = "dá para percorrer do começo ao fim, uma vez";
`range_value_t<Range>` = "o tipo dos elementos"; `predicate<Pred, X>` = "`Pred` pode ser
chamado com um `X` e o resultado serve como condição". O contrato inteiro está na assinatura.

### Policy-based design

Uma **policy** é um tipo passado como parâmetro de template só para **escolher um
comportamento** — o análogo em compile-time de injetar uma dependência. O concept é o que
transforma "espero que esse tipo tenha `log()`" em contrato verificado:

```cpp
template <typename L>
concept LoggingPolicy = requires(L logger, std::string_view msg) {
    logger.log(msg);
    { logger.prefixo() } -> std::convertible_to<std::string>;
};

template <LoggingPolicy Logger = LogConsole>
class ProcessadorDados {
    Logger logger_;
public:
    std::vector<int> processar(std::vector<int> dados) {
        logger_.log(fmt::format("processando {} elementos", dados.size()));
        std::sort(dados.begin(), dados.end());
        return dados;
    }
};
```

```
ProcessadorDados<LogConsole>:
  [LOG] processando 5 elementos
  [LOG] ordenamento concluído
resultado: 1 2 5 8 9

ProcessadorDados<LogSilencioso>:
resultado (sem log): 1 2 5 8 9

LoggingPolicy<LogConsole>:    true
LoggingPolicy<LogSilencioso>: true
LoggingPolicy<int>:           false
```

`LogSilencioso::log` tem corpo vazio: o otimizador remove a chamada inteira. Com uma
interface virtual, a chamada existiria e custaria um salto indireto — comparação detalhada em
[`src/type_erasure/`](../type_erasure/).

### Concepts × herança virtual

|  | Herança virtual | Concepts |
|---|---|---|
| Polimorfismo | runtime | compile-time |
| Coleção heterogênea (`vector<Base*>`) | ✓ | ✗ |
| Tipos de terceiros | ✗ (precisam herdar) | ✓ (duck typing) |
| Custo por objeto | ponteiro de vtable | zero |
| Onde o erro aparece | runtime | compile-time |
| Use quando... | dispatch em runtime, plugins, coleções mistas | templates genéricos, algoritmos, policies |

Nem sempre é escolha: se você precisa guardar objetos de tipos diferentes na mesma coleção,
concepts não resolvem — é `virtual` ou *type erasure*.

---

## Armadilhas que o compilador não pega para você

> ⚠ **1. Um concept restringe a ASSINATURA, não o corpo.** Nada verifica se o corpo do
> template usa apenas o que o concept promete. Se `Somavel` só exige `a + b` e o corpo faz
> `a * b`, isso compila e falha só quando alguém instancia com um tipo sem `*`. O concept é
> documentação **verificada do lado do chamador**, não do lado do autor.

> ⚠ **2. Concepts checam SINTAXE, não SEMÂNTICA.** `std::string` satisfaz qualquer concept
> "tem `operator+`" — e `+` ali concatena, não soma. `std::regular`, `std::totally_ordered` e
> companhia têm requisitos semânticos escritos no padrão que **nenhum compilador verifica**.
> Nomeie os concepts pelo significado e documente o que o `+` deve fazer.

> ⚠ **3. Teste seus concepts nos dois sentidos.** Um concept sempre-verdadeiro (armadilha do
> `requires` faltando) passa despercebido para sempre. Ancore-o com asserções:
> ```cpp
> static_assert( Numerico<int> );
> static_assert( Numerico<double> );
> static_assert(!Numerico<const char*> );
> static_assert(!Numerico<std::string> );
> ```
> Custa zero em runtime e quebra o build no dia em que alguém afrouxar a definição.

> ⚠ **4. Concept não é `static_assert`.** Uma constraint não satisfeita **remove** o
> candidato da sobrecarga; a mensagem final é `no matching function`. Se você quer um erro
> com texto seu, use `static_assert(Concept<T>, "explicação")` **dentro** de um template não
> restrito.

---

## Regras de ouro

| Situação | Escreva |
|---|---|
| restrição simples e com nome | `template <MeuConcept T>` |
| condição de uma expressão só, sem nome | cláusula `requires (sizeof(T) == 4)` |
| parâmetro genérico curto | `auto` restringido — `void f(std::integral auto x)` |
| a mesma restrição em 2+ lugares | dê nome a ela: `concept`, nunca copiar-colar |
| quer que a subsumption funcione | componha concepts **nomeados**; nunca repita type traits |
| exigir que um predicado seja **verdadeiro** dentro de `requires{}` | `requires <predicado>;` — o `requires` interno não é opcional |
| alternativa entre requisitos | `\|\|` **entre concepts**, fora do `requires{}` |
| código novo em C++20 | `concept`, nunca `enable_if` |
| precisa de coleção heterogênea | não é concept — é `virtual` ou type erasure |

---

## Como validar

Compilação isolada, sem passar pelo Meson (o `-I`/`-L` do `fmt` vêm do `.pc` gerado pelo
Conan em `build/`):

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/concepts/main.cpp -o /tmp/cc -L$FMTP/lib -lfmtd && /tmp/cc
```

Estado atual: **sem nenhum warning** com `-Wall -Wextra -Wpedantic`, e a última linha da
saída é `✓ Todos os demos de concepts concluidos!`.

Pelo build do repositório:

```bash
make build                                   # configure + compile + install
./dist/bin/concepts
```

Ou recompilando só este alvo (o `source` é obrigatório: sem ele o Meson não acha o `fmt`):

```bash
source build/conanbuild.sh
meson compile -C build concepts
./build/src/concepts/concepts                # binário do build dir
```

### Reproduzindo as duas mensagens de erro da seção "Por que estudar isto?"

```bash
cat > /tmp/sfinae.cpp <<'EOF'
#include <type_traits>
#include <string>
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
soma(T a, T b) { return a + b; }
int main() { soma(std::string("a"), std::string("b")); }
EOF

cat > /tmp/conc.cpp <<'EOF'
#include <concepts>
#include <string>
template <std::integral T>
T soma(T a, T b) { return a + b; }
int main() { soma(std::string("a"), std::string("b")); }
EOF

g++ -std=c++20 -fsyntax-only /tmp/sfinae.cpp   # muro do enable_if
g++ -std=c++20 -fsyntax-only /tmp/conc.cpp     # "constraints not satisfied"
```

Os dois falham — é o esperado. O que muda é **o que a última linha de cada um diz**.

### Vendo a armadilha do `requires` faltando

```bash
cat > /tmp/trap.cpp <<'EOF'
#include <concepts>
#include <string>
template <typename T> concept Errado = requires { std::integral<T>; };
static_assert(!Errado<std::string>, "o concept passou quando nao devia");
EOF
g++ -std=c++20 -Wall -fsyntax-only /tmp/trap.cpp
```

Esperado: o `static_assert` **falha** e o GCC emite
`warning: ... add 'requires' to check satisfaction [-Wmissing-requires]`. É a armadilha nº 1
da seção anterior, ao vivo.

---

## Referências

Dentro deste repositório:

- [`src/templates/`](../templates/) — SFINAE, `enable_if`, especialização: o que os concepts
  substituem
- [`src/ranges/`](../ranges/) — a maior aplicação de concepts na biblioteca padrão
- [`src/compile_time/`](../compile_time/) — `constexpr`, `consteval`, `if constexpr`: o resto
  do que acontece antes do programa rodar
- [`src/type_erasure/`](../type_erasure/) — o outro caminho quando concepts não bastam
- [`src/stl/`](../stl/) — os contêineres que os concepts deste subprojeto descrevem

Fora dele:

- cppreference — *Constraints and concepts* e o cabeçalho `<concepts>`
- **P0734R0** (Sutter, Dos Reis, Stroustrup, Reis) — *Wording Paper, C++ extensions for
  Concepts*: a proposta que entrou no C++20
- **N4553** — *Concepts TS*, útil para entender por que a sintaxe final ficou como ficou
- *C++ Core Guidelines*, seção **T (Templates and generic programming)**, itens T.10–T.25 —
  em especial T.11 ("prefira concepts padronizados") e T.20 ("evite 'concepts' sem
  significado semântico")
- GCC: `-fconcepts-diagnostics-depth=2` faz o compilador abrir a cadeia de satisfação inteira
  quando a mensagem padrão para cedo
