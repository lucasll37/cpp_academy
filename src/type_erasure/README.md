# type_erasure — Apagamento de Tipo

## O que é este projeto?

**Type erasure** é guardar um objeto de tipo `T` num lugar que **não menciona `T`**,
preservando as operações que interessam. O tipo some da assinatura; o comportamento fica.

Você já usa isso sem chamar pelo nome:

```cpp
std::function<int(int)> f;   // aceita lambda, functor, ponteiro de função...
std::any                a;   // aceita qualquer coisa copiável
std::shared_ptr<void>   p;   // guarda o deleter de QUALQUER tipo
std::unique_ptr<Base>   b;   // a herança é a forma mais antiga
```

Este subprojeto abre as três caixas: **o que é**, **como se implementa** e **quando não vale
a pena** — esta última respondida com medição, não com opinião.

## Por que estudar isto?

Porque é o mecanismo por trás de metade da biblioteca padrão, e porque a alternativa que a
maioria conhece — herdar de uma base comum — tem um custo de projeto que raramente é
contabilizado: o tipo passa a ter vptr, deixa de ser trivial, não cabe mais num registrador,
precisa de heap e ponteiro, e não pode ser `int` nem um tipo de terceiros.

Três resultados **medidos** aqui:

| Medição | Resultado |
|---|---|
| SBO da `std::function` | captura de **16 B cabe inline; 24 B aloca** |
| despacho sobre 200 000 elementos | `variant` **1.0×**, `virtual` **3.9×**, type erasure **4.0×** (base: CRTP) |
| o que domina esse 4× | **localidade**, não o despacho |

## Estrutura de arquivos

```
src/type_erasure/
├── main.cpp                    ← ponto de entrada, cor coral
├── demo_conceito.hpp           ← 1 · o que é, e a herança como caso particular
├── demo_implementacao.hpp      ← 2 · std::function do zero + SBO medido
├── demo_any.hpp                ← 3 · any, o deleter do shared_ptr, type_index
├── demo_alternativas.hpp       ← 4 · virtual vs CRTP vs variant vs erasure
├── demo_pratica.hpp            ← 5 · polimorfismo com semântica de valor
├── meson.build                 ← C++23 (por causa de move_only_function)
└── README.md
```

```bash
make build && ./dist/bin/type_erasure
```

---

## Demo 1: O que é

### O problema

Três structs que não se conhecem e não herdam de nada. Como guardar os três num único
`std::vector`? Não há resposta boa dentro do sistema de tipos estático: `vector<Circulo>` só
guarda círculos, um template é resolvido em compilação, e `void*` perde inclusive **como
destruir**.

### A herança já é type erasure — só que intrusiva

`std::unique_ptr<Forma>` não menciona `Circulo` nem `Quadrado`. O tipo foi apagado; a vtable é
o mecanismo. O defeito é o "intrusiva": para entrar no vetor, o tipo precisa **herdar**. Isso
exclui `int`, `std::string` e qualquer tipo de biblioteca que você não pode alterar — e
acrescenta vptr, heap e risco de *object slicing* ao seu tipo.

### As três formas

| forma | conjunto de tipos | despacho |
|---|---|---|
| herança + `virtual` | **aberto**, intrusivo | vtable |
| `std::variant` + `visit` | **fechado** | índice / jump table |
| type erasure manual | **aberto**, não intrusivo | vtable manual |

---

## Demo 2: `std::function` por dentro

### A receita — três peças, sempre as mesmas

```cpp
template <typename R, typename... Args>
class Funcao<R(Args...)> {
    struct Conceito {                                  // 1. INTERFACE: o quê
        virtual ~Conceito() = default;
        virtual R invocar(Args...) const = 0;
        virtual std::unique_ptr<Conceito> clonar() const = 0;
    };
    template <typename T> struct Modelo final : Conceito {   // 2. MODELO: com o quê
        T alvo;
        R invocar(Args... a) const override { return const_cast<T&>(alvo)(a...); }
        std::unique_ptr<Conceito> clonar() const override {
            return std::make_unique<Modelo>(alvo);
        }
    };
    std::unique_ptr<Conceito> pimpl_;                  // 3. WRAPPER: valor semântico
};
```

O truque está na peça 2: `Modelo<T>` conhece `T`, herda de `Conceito` e some atrás do
ponteiro. **Quem escreveu `T` nunca ouviu falar de nenhuma das três.**

> ⚠ O construtor template precisa de `requires (!std::is_same_v<std::decay_t<T>, Funcao>)`.
> Sem isso ele sequestra a cópia: `Funcao f2{f1}` construiria uma `Funcao<Funcao<...>>`.

### Por que `clonar()`

`unique_ptr<Conceito>` não é copiável — sem `clonar()`, a `Funcao` deixaria de ser um **valor**.
Três saídas:

| | efeito |
|---|---|
| `clonar()` virtual | cópia profunda; é o padrão *Prototype*, e o que `std::function` faz |
| `shared_ptr<const Conceito>` | cópia rasa e barata, mas o alvo passa a ser compartilhado |
| proibir a cópia | é o `std::move_only_function` do C++23 |

`std::function` escolheu a primeira e paga o preço: **exige** que o callable seja copiável.
Uma lambda que captura um `unique_ptr` por move não cabe nela.

### SBO — medido, não presumido

Substituindo o `operator new` global por um instrumentado:

```
std::function, sem captura → 0 alocações, INLINE no buffer
std::function, captura 8B  → 0 alocações, INLINE no buffer
std::function, captura 16B → 0 alocações, INLINE no buffer
std::function, captura 24B → 1 alocação, 24 bytes
std::function, captura 64B → 1 alocação, 64 bytes
v1::Funcao (a nossa)       → 1 alocação, SEMPRE
```

`sizeof(std::function<void()>)` = 32 bytes: ponteiro de vtable, ponteiro de invocação e 16
bytes de buffer.

A regra exata é da **implementação**, não do padrão. A libstdc++ ainda exige que o callable
seja *nothrow-move-constructible* para usar o buffer — se o move puder lançar, ela aloca,
porque mover a `function` teria que mover o buffer.

> **Prática:** uma lambda que captura `[this]` ou um `int` é gratuita dentro de
> `std::function`. Uma que captura três `std::string` por valor aloca.

> ⚠ A substituição do `operator new` global precisa estar no **escopo global** (dentro de um
> namespace é erro de compilação) e ser **única no programa** — por isso não é `inline`, e o
> cabeçalho só pode ser incluído por uma unidade de tradução. Também dispara um
> `-Wmismatched-new-delete` falso-positivo ao inlinar, suprimido localmente com pragma.

### Vtable manual, sem herança

A `v1` usa `virtual`. Dá para apagar o tipo **sem herança nenhuma**: um `struct` de ponteiros
de função, `static constexpr`, uma instância por tipo `T`.

| aspecto | `virtual` | vtable manual |
|---|---|---|
| ponteiro no **objeto** | sim (vptr) | **não** |
| permite SBO | difícil | **sim** |
| escolher as operações | fixo | por chamada |
| legibilidade | melhor | pior |

Sem vptr, o objeto guardado pode ir direto para o buffer interno — é por isso que a libstdc++
escolhe esta forma.

---

## Demo 3: O apagamento que já vem no padrão

Cada apagador do padrão preserva um conjunto diferente de operações:

| tipo | preserva |
|---|---|
| `std::function` | **chamar** |
| `std::any` | destruir e copiar (nem imprimir dá) |
| `std::shared_ptr<T>` | **destruir certo** |
| `std::move_only_function` | chamar, sem exigir cópia (C++23) |
| `std::type_index` | identidade de tipo em runtime |

### `std::any`

Recuperar exige o tipo **exato** — `any_cast<long>` num `int` lança `bad_any_cast`. Não há
conversão: a comparação é `typeid` exato. A forma de ponteiro (`any_cast<T>(&a)`) devolve
`nullptr` em vez de lançar.

> **Quando usar:** quase nunca. Antes, veja se um `variant` serve (lista fechada, mas com
> `visit`) ou se um apagamento sob medida serve. `any` é para guardar algo e devolvê-lo intacto
> **sem nunca olhar dentro**: um blob de config, uma *user data* repassada a um callback.

### ⚠ O deleter apagado do `shared_ptr`

O exemplo mais usado e menos percebido:

```cpp
std::unique_ptr<T, D>   ← o deleter está NO TIPO
std::shared_ptr<T>      ← e aqui, cadê?
```

Está no **bloco de controle**, com o tipo apagado. Três consequências:

1. **`shared_ptr<Base>` destrói corretamente mesmo sem destrutor virtual** — o deleter foi
   capturado quando o tipo ainda era o derivado. Verificado no demo: `~Contada` roda.
   ⚠ Isto **não** é licença para omitir o destrutor virtual: só funciona se o `shared_ptr` foi
   criado a partir do tipo derivado.
2. **Recursos de C sem esforço:** `std::shared_ptr<FILE> f{fopen(...), &fclose}` — o tipo
   continua `shared_ptr<FILE>`.
3. **O *aliasing constructor*** guarda um ponteiro e mantém vivo **outro** objeto:
   `shared_ptr<int>{todo, &todo->parte}`.

O preço: o bloco de controle é uma alocação (evitável com `make_shared`) e uma indireção a
mais na destruição.

### `std::move_only_function` (C++23)

Aceita a lambda que captura por move — e ainda acertou os qualificadores da assinatura:

```cpp
move_only_function<int() const>     → operator() é const de verdade
move_only_function<int() &&>        → só invocável em rvalue
move_only_function<int() noexcept>  → propaga o noexcept
```

`std::function` faz um `const_cast` interno (como a nossa `v1`) e **mente sobre
const-correção**.

### `std::type_index`

`std::type_info` não é copiável nem tem `operator<`. `type_index` é o wrapper que o torna
chave de `map` — a base do despacho dinâmico **aberto**: qualquer módulo registra um handler
novo sem tocar em nada. É como funciona o [`plugin_registry`](../plugin_registry/).

Custo: uma busca em `std::map` por chamada, contra uma indireção só na vtable.

> ⚠ `typeid(T).name()` é **manglado** e específico do compilador. Não serialize esse nome.

---

## Demo 4: A medição que decide

200 000 elementos, média de 20 rodadas, `-O2`:

```
virtual (unique_ptr<Base>)           0.514 ms/rodada    3.9×
CRTP (vetores homogêneos)            0.130 ms/rodada    1.0×
variant + visit                      0.131 ms/rodada    1.0×
type erasure (unique_ptr interno)    0.517 ms/rodada    4.0×
```

**A diferença que domina não é o despacho — é a localidade.** `variant` e CRTP percorrem
memória contígua; `virtual` e type erasure percorrem um vetor de ponteiros para lugares
espalhados no heap. Um cache miss custa ~200 ciclos; uma chamada indireta bem predita, ~2.

Ver [`src/performance/`](../performance/) para essa medição isolada.

### O problema da expressão

Você tem **tipos** e **operações**. Só dá para deixar um dos dois eixos barato de estender:

| abordagem | tipo novo | operação nova |
|---|---|---|
| `virtual` | fácil: 1 classe | **caro**: mexe na base e em todo derivado |
| `variant` + `visit` | **caro**: recompila todos os visitors | fácil: outro visitor |
| type erasure | fácil, e sem tocar no tipo | **caro**: mexe no `Conceito` |
| CRTP | fácil | fácil — *mas sem heterogeneidade em runtime* |

O CRTP parece ganhar dos dois lados porque **não resolve o mesmo problema**: ele não dá um
container heterogêneo.

### Como decidir, na ordem

1. Tipos conhecidos, lista curta e estável? → **`variant`**. Contíguo, sem alocação,
   exaustividade checada em compilação.
2. Tipos abertos e você controla as classes? → **herança + `virtual`**. É o mais legível.
3. Tipos abertos e você **não** controla as classes (`int`, `std::string`, terceiros)? →
   **type erasure**.
4. Sem heterogeneidade em runtime, só reuso de código? → **template/CRTP**, e nem pense em vtable.

---

## Demo 5: Polimorfismo com semântica de valor

A crítica de Sean Parent (*Inheritance Is The Base Class of Evil*, GoingNative 2013): herdar
para ser polimórfico contamina o seu tipo com ponteiro, heap e aliasing — problemas que ele
não tinha.

A alternativa: **o polimorfismo mora no container, não no tipo guardado**.

```cpp
std::vector<Desenhavel> cena;          // por VALOR, não unique_ptr
cena.emplace_back(Ponto{1, 2});
cena.emplace_back(Texto{"olá"});
cena.emplace_back(Grupo{"sub", {Ponto{9,9}, Texto{"aninhado"}}});

std::vector<Desenhavel> copia = cena;  // cópia PROFUNDA — nem compilaria com unique_ptr
```

`Ponto`, `Texto` e `Grupo` são structs comuns: sem vptr, copiáveis, usáveis fora de qualquer
cena.

| aspecto | `unique_ptr<Base>` | `Desenhavel` |
|---|---|---|
| copiável | **não** | sim |
| o tipo precisa herdar | sim | **não** |
| funciona com `int`/`string` | não | se atender ao concept |
| pode ser `nullptr` | sim | **não** |
| `==` e `<` | compara **ponteiro** | pode ser por valor |
| aloca | sim | sim |

"Pode ser `nullptr`" merece atenção: `Desenhavel` não tem construtor padrão, então não existe
"desenhável vazio". Elimina uma classe inteira de bugs — o mesmo argumento de `optional<T>`
vs `T*`.

### O concept como contrato

```cpp
template <typename T>
concept SabeDesenhar = requires(const T& t, int nivel) {
    { t.desenhar(nivel) } -> std::same_as<void>;
    { t.nome() }          -> std::convertible_to<std::string>;
};
```

Sem ele, um tipo inadequado gera um erro **dez níveis de template abaixo**, dentro de
`Modelo<T>`. Com ele, o erro volta para a linha do usuário e diz qual expressão faltou. E o
concept vira documentação executável: "o que preciso implementar para caber aqui?" tem
resposta em 4 linhas de código.

### ⚠ Quando NÃO fazer isto

- **Você controla todos os tipos e a lista é curta** → `variant` é 4× mais rápido, não aloca e
  checa exaustividade.
- **O conjunto de operações é grande ou instável** → cada operação nova entra em três lugares.
  Uma interface de 20 métodos vira 60 linhas de repasse mecânico.
- **Não há heterogeneidade em runtime** → um template resolve em compilação e some.
- **`virtual` já resolve e é mais legível** → type erasure é ~40 linhas de boilerplate para o
  que `virtual` faz em 3.

Bibliotecas que geram o boilerplate: `dyno` (Louis Dionne), `Boost.TypeErasure`, `folly::Poly`.
Sem reflexão na linguagem, todas custam macro ou muita metaprogramação — o C++26 deve mudar
esse quadro.

---

## Como validar

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/type_erasure/main.cpp -o /tmp/te -L$FMTP/lib -lfmtd

g++ -std=c++23 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/type_erasure/main.cpp -o /tmp/tesan -L$FMTP/lib -lfmtd && /tmp/tesan
```

Estado atual: sem warnings com `-Wall -Wextra -Wpedantic`, e **0 achados sob ASan+UBSan**.

## Referências

- [`src/oop/`](../oop/) — vtable, vptr e slicing: o mecanismo por baixo
- [`src/vocabulary_types/`](../vocabulary_types/) — `variant`, `any`, overload idiom
- [`src/templates/`](../templates/) — CRTP e polimorfismo estático
- [`src/concepts/`](../concepts/) — o contrato explícito
- [`src/lambdas/`](../lambdas/) — `std::function` vs template, do lado do usuário
- [`src/design-of-pattern/visitor/`](../design-of-pattern/visitor/) — o problema da expressão
- Sean Parent, *Inheritance Is The Base Class of Evil* (GoingNative 2013)
- Klaus Iglberger, *Breaking Dependencies: Type Erasure* (CppCon 2021/2022)
