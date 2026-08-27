# vocabulary_types — optional, variant, any, tuple e span

## O que é este projeto?

Um guia sobre os **tipos de vocabulário** da biblioteca padrão: os tipos que existem para
serem *falados* entre bibliotecas, não para resolver um domínio específico. Se duas
bibliotecas que nunca se viram precisam dizer "aqui pode não haver valor" ou "isto é um
destes três tipos", elas usam estes tipos — do mesmo jeito que usam `int` e `std::string`.

Cada um responde a uma pergunta diferente sobre *"que valor está aqui?"*:

| Tipo | Responde | Os tipos são conhecidos… |
|---|---|---|
| `optional<T>` | *pode não haver valor* | na compilação, 1 tipo |
| `variant<Ts...>` | *é um destes* | na compilação, N tipos |
| `any` | *é qualquer coisa* | só em **runtime** |
| `tuple<Ts...>` | *são todos estes* | na compilação, N tipos |
| `span<T>` | *é uma janela sobre aqueles* | na compilação, 1 tipo |

Para compilar e rodar tudo:

```bash
make build && ./dist/bin/vocabulary_types
```

> Se `dist/bin/` ainda não existir, rode `make all` uma vez: `build/` é o diretório de
> compilação e `dist/` é o **prefixo de instalação**, e é `make install` (embutido no
> `make all`) que copia os executáveis para `dist/bin/`.

> ⚠ Este alvo compila com **C++23** — as operações monádicas de `optional`
> (`and_then`/`transform`/`or_else`) chegaram nesse padrão. O `cpp_std` da raiz do
> repositório continua C++20; a exceção é local, via `override_options` no
> [meson.build](meson.build).

---

## Antes de começar

Esta seção define **todos** os termos usados adiante. Se você já conhece algum, pule.

**Tipo de vocabulário.** Um tipo da biblioteca padrão cuja única função é ser entendido por
todo mundo. `std::optional<int>` na fronteira entre duas bibliotecas dispensa qualquer
combinado por escrito: o significado está no tipo.

**UB (*undefined behavior*, comportamento indefinido).** Uma operação para a qual o padrão
do C++ **não impõe nenhuma exigência**. O programa pode devolver lixo, travar, ou parecer
funcionar — e o compilador tem licença para otimizar assumindo que aquilo nunca acontece.
Não é "erro em runtime": é a ausência de qualquer garantia. Ver
[`src/undefined_behavior/`](../undefined_behavior/).

**`union`.** Construção herdada do C: vários membros que ocupam **o mesmo espaço** de
memória, um de cada vez. A union não guarda qual deles está ativo; ler um membro diferente
do que foi escrito é UB. É o problema que `variant` resolve.

**Range.** Qualquer coisa que tenha `begin()` e `end()` — um par de posições que delimita
uma sequência. É o conceito que a biblioteca `<ranges>` usa no lugar de dois iteradores
soltos. Um `std::vector`, um array C e um `std::span` são todos ranges; por isso funcionam
em `for (auto x : ...)`. Ver [`src/ranges/`](../ranges/).

**`borrowed_range`.** Um range cujos **iteradores continuam válidos mesmo depois de o
objeto range ser destruído** — porque ele não possui os dados, só aponta para eles.
`span` e `string_view` são borrowed; `vector` não é (quando o vector morre, os dados morrem
junto). Detalhes no Demo 5.

**Extent.** O número de elementos de um `span`, **quando ele é conhecido no tipo**.
`std::span<int, 3>` tem extent estático 3; `std::span<int>` tem extent *dinâmico*, isto é,
o tamanho é um campo lido em runtime.

**SBO (*Small Buffer Optimization*).** Um buffer de poucos bytes **dentro do próprio
objeto**. Se o valor guardado couber nele, não há `malloc`; se não couber, vai para a heap.
É como `std::any` evita alocar para tipos pequenos. Medido no Demo 3.

**Operação monádica.** Uma operação que recebe uma função e a aplica **só se houver valor**,
propagando a ausência sozinha. `and_then`, `transform` e `or_else` são as três de
`optional`. O nome vem da teoria de categorias; o que importa aqui é o contrato, tabelado
no Demo 1.

***Structured bindings*** (C++17). A sintaxe `auto [a, b] = par;`, que dá **nomes novos** aos
pedaços de uma tuple, de um pair, de um array ou de um struct simples. Os nomes são
variáveis novas, não apelidos: `auto [a,b]` copia, `auto& [a,b]` referencia.

***Fold expression*** (C++17). A sintaxe `(f(e), ...)`, que manda o compilador **repetir** o
código uma vez para cada elemento de um pacote variádico, em compilação. É como se percorre
uma tuple, já que não existe laço sobre tipos diferentes. Ver
[`src/templates/`](../templates/).

**Rule of 5.** Se uma classe precisa de destrutor, construtor de cópia ou operador de
atribuição customizados, provavelmente precisa dos cinco (destrutor, cópia, move,
atribuição por cópia, atribuição por move). Aparece aqui porque um move `noexcept` é o que
impede o estado `valueless` do `variant`. Ver [`src/value_semantics/`](../value_semantics/).

**pimpl** (*pointer to implementation*). Guardar um ponteiro para uma struct de
implementação declarada só no `.cpp`, para que o cabeçalho público não revele nada. Aparece
aqui como um dos poucos usos legítimos de `any`.

---

## Por que estudar isto?

Porque as alternativas que esses tipos substituem são todas **convenções não verificáveis** —
combinados que existem só na cabeça de quem escreveu, e que o compilador não cobra:

| Convenção antiga | Problema |
|---|---|
| ponteiro nulo para "sem valor" | nada no tipo exige a verificação |
| `union` + enum de tag | ler o membro errado é UB, e nada impede |
| `void*` | perde o tipo por completo |
| `(ponteiro, tamanho)` | dois parâmetros que precisam concordar |
| `bool` de saída + parâmetro de saída | fácil de ignorar, saída indefinida em falha |

Em cada linha, o defeito é o mesmo: a informação existe, mas **não está no tipo**, então o
compilador não pode ajudar.

---

## Estrutura de arquivos

```
src/vocabulary_types/
├── main.cpp             ← ponto de entrada, executa os 5 demos
├── demo_optional.hpp    ← 1 · ausência no tipo, monádicas, optional<bool>, optional<T&>
├── demo_variant.hpp     ← 2 · visit, overload idiom, valueless, monostate, AST recursiva
├── demo_any.hpp         ← 3 · any_cast, alocações CONTADAS, quando não usar
├── demo_tuple.hpp       ← 4 · structured bindings, tie, apply, percorrer
├── demo_span.hpp        ← 5 · extent estático vs dinâmico, subspan, borrowed_range
├── meson.build
└── README.md
```

---

## Demo 1: `std::optional`

### O exemplo mínimo

Um programa completo. Salve como `t.cpp` e compile com `g++ -std=c++23 t.cpp -o t && ./t`:

```cpp
#include <cstdio>
#include <optional>

// A ausência de valor está no TIPO de retorno — impossível ignorar.
std::optional<int> metade_se_par(int n) {
    if (n % 2 != 0) return std::nullopt;   // "não há valor"
    return n / 2;                          // converte sozinho para optional<int>
}

int main() {
    if (auto r = metade_se_par(4)) {       // operator bool = "tem valor?"
        std::printf("metade de 4  = %d\n", *r);
    }
    std::printf("metade de 7  = %d  (value_or)\n", metade_se_par(7).value_or(-1));
    std::printf("has_value(7) = %d\n", metade_se_par(7).has_value());
}
```

```
metade de 4  = 2
metade de 7  = -1  (value_or)
has_value(7) = 0
```

As três formas de **criar** aparecem todas aí: `std::nullopt` para o vazio, `return n / 2;`
para o cheio (a conversão é implícita), e `std::optional<int> x;` (sem inicializador) também
nasce vazio.

### As três formas de acessar

| Forma | Se estiver vazio | Quando usar |
|---|---|---|
| `*opt` | **undefined behavior** | você já testou com `if` |
| `opt.value()` | lança `std::bad_optional_access` | quer rede de segurança |
| `opt.value_or(x)` | devolve `x` | tem um default óbvio |

> ⚠ `*opt` num optional vazio **não** é exceção nem crash garantido — é **lixo silencioso**.
> Teste antes, sempre.

### Nenhuma alocação

Números impressos pelo demo nesta máquina (x86-64, libstdc++):

```
sizeof(int)              =  4
sizeof(optional<int>)    =  8      ← int + flag de "tem valor" + padding
sizeof(std::string)      = 32
sizeof(optional<string>) = 40
&optional  = 0x7ffd81b211b0
&*optional = 0x7ffd81b211b0        ← o T vive DENTRO do optional
```

`optional` **não é um ponteiro**. É o valor mais um `bool`, num buffer alinhado dentro do
próprio objeto. `emplace(args...)` constrói o valor no lugar, sem temporário; `reset()`
destrói o valor e volta ao estado vazio.

### Operações monádicas (C++23)

**O problema primeiro.** Encadear três funções que podem falhar, sem monádicas:

```cpp
auto n = nome_do_id(id);
if (!n) return -1;                       // propagação 1
auto c = comprimento_se_curto(*n);
if (!c) return -1;                       // propagação 2
return *c * 100;
```

Cada passo novo acrescenta um `if` que não faz nada além de repassar a ausência. As
monádicas fazem esse repasse sozinhas:

```cpp
nome_do_id(id)                                       // optional<string>
    .and_then(comprimento_se_curto)                  // optional<int>
    .transform([](int n) { return n * 100; })        // optional<int>
    .or_else([] { return std::optional<int>{-1}; }); // nunca vazio no fim
```

O contrato de cada uma:

| Operação | A função recebe | A função devolve | Papel |
|---|---|---|---|
| `and_then(f)` | `T` | `optional<U>` | encadeia outra operação que pode falhar |
| `transform(f)` | `T` | `U` | mapeia o valor; o optional é reembrulhado |
| `or_else(f)` | (nada) | `optional<T>` | só roda **se estiver vazio**: fornece o default |

Em todas, se o optional estiver vazio a função **não é chamada** e o vazio segue adiante.
A mesma ideia vale para `std::expected` — ver [`src/errors/`](../errors/).

### Por que `optional<T&>` não existe

`opt = outro` seria ambíguo: **religar** a referência (fazê-la apontar para outro objeto) ou
**atribuir** ao objeto referido? As duas leituras são defensáveis, o comitê não escolheu,
então proibiu. Na prática, `T*` já é o "optional reference" do C++: `nullptr` é a ausência.

Quando a assinatura precisa dizer "opcional" de forma inequívoca, existe
`std::optional<std::reference_wrapper<const T>>`, que compila — mas custa um `.get()` em
cada uso.

### ⚠ `optional<bool>`

Tem **três** estados, e `if (o)` responde *"tem valor?"*, não *"o valor é true?"*:

```
vazio        if(o) = false   value_or(false) = false
false        if(o) = true    value_or(false) = false    ← a armadilha
true         if(o) = true    value_or(false) = true
```

A linha do meio é a que pega todo mundo: o optional contém `false`, e mesmo assim `if (o)` é
verdadeiro. Prefira `enum class Resposta { Sim, Nao, NaoInformado }`; se mantiver o
`optional<bool>`, escreva sempre `if (o.has_value() && *o)`.

---

## Demo 2: `std::variant`

### O exemplo mínimo

```cpp
#include <cstdio>
#include <string>
#include <variant>

template <class... Ts> struct Overload : Ts... { using Ts::operator()...; };

int main() {
    std::variant<int, std::string> v = std::string("olá");
    std::printf("index = %zu\n", v.index());        // 1 = o segundo tipo da lista
    std::visit(Overload{
        [](int i)                { std::printf("inteiro %d\n", i); },
        [](const std::string& s) { std::printf("texto '%s'\n", s.c_str()); },
    }, v);
}
```

```
index = 1
texto 'olá'
```

### O que muda em relação à `union`

```
union: escrevi u.i = 42
  ler u.d → seria UB, e NADA no código impede

variant: v = 42
  v.index()                 = 0   ← o variant SABE quem está ativo
  holds_alternative<double> = false
  std::get<double>(v)       → lança bad_variant_access
```

Medido: `sizeof(UnionCrua) = 8` e `sizeof(variant<int,double,char>) = 16`. O variant paga um
índice (mais alinhamento) e devolve segurança de tipo. Ele **nunca** aloca: reserva inline o
espaço do maior alternativo.

### As quatro formas de acessar

| Forma | Se o tipo estiver errado | Exaustividade | Observação |
|---|---|---|---|
| `get_if<T>(&v)` | devolve `nullptr` | não checa | não lança; o teste fica com você |
| `get<T>(v)` | lança `bad_variant_access` | não checa | só funciona se `T` for único na lista |
| `get<I>(v)` | lança `bad_variant_access` | não checa | acesso por **índice**; é a saída quando há tipos repetidos, como em `variant<int,int>` |
| `visit(f, v)` | não acontece | **garantida** | o compilador exige um caso para cada tipo |

Prefira `visit`: é o único em que **esquecer um caso é erro de compilação**.

### O overload idiom

Duas linhas de biblioteca que tornam `visit` legível:

```cpp
template <typename... Ts>
struct Overload : Ts... { using Ts::operator()...; };

std::visit(Overload{
    [](int i)                { return fmt::format("inteiro {}", i); },
    [](const std::string& s) { return fmt::format("texto '{}'", s); },
}, v);
```

O struct herda de cada lambda e o `using Ts::operator()...;` traz o `operator()` de todos
para o mesmo escopo — a resolução de overload comum faz o resto. Acrescentar um tipo ao
variant sem acrescentar o lambda correspondente **não compila**.

### Despacho duplo de graça

`visit` aceita **vários** variants e o compilador gera a matriz de combinações. O demo
resolve pedra-papel-tesoura com sete lambdas. É o padrão Visitor do GoF sem hierarquia, sem
`accept()` e sem vtable.

### `valueless_by_exception`

Se a construção do novo valor lançar **durante** uma atribuição, o antigo já foi destruído e
o novo não existe:

```
antes:  index=0 valueless=false
exceção durante a atribuição: falhei na cópia
depois: index=-1 valueless=true      ← index() devolveu std::variant_npos
```

Nesse estado, `visit` lança `bad_variant_access`. É o único estado inválido que um variant
pode ter, e é raro. Como evitar: primeiro alternativo trivial (ou `monostate`), preferir
`emplace` a atribuição, e tipos com move `noexcept` — a
[Rule of 5](../value_semantics/) de novo.

### `std::monostate`

Um `variant` só tem construtor default se o **primeiro** alternativo tiver. Quando ele não
tem, `std::monostate` resolve: é um struct vazio (`sizeof == 1`) que existe apenas para dar
ao variant uma primeira alternativa trivialmente construtível — e, de quebra, um estado
"nada ainda" legítimo.

```cpp
struct SemDefault { explicit SemDefault(int) {} };

// std::variant<SemDefault, int> v;                 // ✗ não compila
std::variant<std::monostate, SemDefault, int> v;    // ✓ index() == 0
```

```
variant<monostate, SemDefault, int> v;  → index=0 (monostate)
após v = 7                              → index=2
voltando a "vazio" (v = monostate{})    → index=0
```

### Variant recursivo — uma AST sem herança

Um variant não pode conter a si mesmo (o tamanho seria infinito); a saída é a indireção, com
`std::unique_ptr` para os nós:

```cpp
struct Soma; struct Multiplicacao;

using Expressao = std::variant<double,
                               std::unique_ptr<Soma>,
                               std::unique_ptr<Multiplicacao>>;

struct Soma          { Expressao esq, dir; };
struct Multiplicacao { Expressao esq, dir; };
```

O demo monta `(2 + 3) * 4`, imprime a expressão e avalia — duas operações, dois `visit`,
zero classes base:

```
expressão: ((2 + 3) * 4)
valor:     20
```

| Critério | `variant` | herança virtual |
|---|---|---|
| tipos novos **externos** | não | **sim** |
| operações novas | **sim** | recompila tudo |
| exaustividade checada | **sim** | não |
| alocação por nó | opcional | sempre |

É o *expression problem*: `variant` favorece **operações** novas, herança favorece **tipos**
novos. Escolha pelo eixo que vai variar mais.

---

## Demo 3: `std::any`

### O exemplo mínimo

```cpp
#include <any>
#include <cstdio>
#include <string>
#include <typeinfo>

int main() {
    std::any a = 42;
    std::printf("guarda um int? %d\n", a.type() == typeid(int));
    std::printf("valor = %d\n", std::any_cast<int>(a));       // lança se errar o tipo
    if (std::any_cast<std::string>(&a) == nullptr) {          // versão ponteiro: não lança
        std::printf("não é string — e não houve exceção\n");
    }
}
```

```
guarda um int? 1
valor = 42
não é string — e não houve exceção
```

### As três formas de `any_cast`

| Forma | Devolve | Se o tipo estiver errado | Custo |
|---|---|---|---|
| `any_cast<T>(a)` | uma **cópia** do valor | lança `std::bad_any_cast` | cópia de `T` |
| `any_cast<T>(&a)` | `T*` para o valor de dentro | devolve `nullptr` | nenhum |
| `any_cast<T&>(a)` | **referência** ao valor de dentro | lança `std::bad_any_cast` | nenhum |

> ⚠ O casamento de tipo é **exato**: um `any` que guarda um `double` não responde a
> `any_cast<int>`. Não há conversão implícita nem relação de herança envolvida.

A forma com ponteiro é a que se usa em condição de `if`, exatamente como `get_if` no variant.

### O que é SBO

Todo `std::any` reserva alguns bytes **dentro do próprio objeto**. Se o `T` couber neles
(e for `noexcept`-movível), o valor mora ali e **não há `malloc`**; se não couber, o `any`
guarda um ponteiro e o valor vai para a heap.

```
        sizeof(std::any) = 16 bytes
  ┌──────────────────┬──────────────────┐
  │ buffer / ponteiro│ manejador (tipo, │
  │     8 bytes      │ cópia, destruição│
  └──────────────────┴──────────────────┘
```

Na libstdc++ o buffer útil é de `sizeof(void*)` = 8 bytes. Daí os números da próxima seção.

### O custo, contado

O demo substitui o `operator new` global por uma versão que **conta** as alocações — em vez
de afirmar, mede:

```
any<Pequeno> ( 4B) → 0 alocação(ões),  0 bytes
any<Medio>   (16B) → 1 alocação(ões), 16 bytes
any<Grande>  (64B) → 1 alocação(ões), 64 bytes
variant<Pequeno,Grande> com Grande → 0 alocação(ões)    ← nunca aloca
```

`Pequeno` tem 4 bytes e cabe no buffer; `Medio` tem 16 e já não cabe. Acima do limite,
**cada** `any` construído é um `malloc` mais um `free` — e num laço quente isso é pressão
sobre o alocador, não apenas alguns nanossegundos.

### A pergunta que elimina o `any` quase sempre

*"Eu consigo listar os tipos possíveis?"*

- **sim** → `variant`. Ganha exaustividade e zero alocação.
- **não, mas todos fazem a mesma coisa** → interface virtual.
- **não, e cada um faz coisa diferente** → aí sim, `any`.

> O sintoma de `any` mal usado é uma cascata de `any_cast` em `else if`. Isso **é** um
> variant, escrito à mão e sem exaustividade.

### Onde `any` é a resposta certa

Quando o conjunto de tipos é **aberto por design** — definido por quem *usa* a biblioteca,
não por quem a escreve. O demo implementa uma classe `Propriedades` (`map<string, any>`)
cujo `obter<T>` devolve `std::optional<T>`, juntando as duas ferramentas: *"pode não
existir"* e *"pode ser de qualquer tipo"*. Outros casos: "user data" de callbacks C,
propriedades de GUI, nós de configuração dinâmica, pimpl genérico.

---

## Demo 4: `tuple` e `pair`

### O exemplo mínimo

```cpp
#include <cstdio>
#include <string>
#include <tuple>

std::tuple<std::string, int, double> obter_registro() { return {"Ada", 36, 9500.0}; }

int main() {
    const auto [nome, idade, salario] = obter_registro();   // structured bindings
    std::printf("%s, %d anos, %.2f\n", nome.c_str(), idade, salario);
}
```

```
Ada, 36 anos, 9500.00
```

### Structured bindings salvaram a tuple

```cpp
auto [nome, idade, salario] = obter_registro();   // ✓ nomes no ponto de uso
if (std::get<0>(r)) ...                           // ✗ ilegível: o que é o campo 0?
```

Antes do C++17 o acesso por índice era o custo real da tuple, e o motivo de todo mundo
preferir struct.

> ⚠ Os nomes são **variáveis novas**, não apelidos. `auto [a,b]` **copia**; `auto& [a,b]`
> referencia; `const auto& [a,b]` é o default seguro.

### `std::tie`

```cpp
return std::tie(a.idade, a.nome) < std::tie(b.idade, b.nome);   // desempate lexicográfico
std::tie(nome, idade, salario) = obter_registro();              // variáveis já existentes
std::tie(std::ignore, so_a_idade, std::ignore) = obter_registro();
```

A primeira linha substitui o `if (a.idade != b.idade) return a.idade < b.idade; return
a.nome < b.nome;` escrito à mão — que, com quatro critérios, vira sete linhas e um bug
esperando.

> ⚠ `tie` guarda **referências** — nunca devolva o resultado de `tie` de uma função, as
> referências ficam penduradas. Para devolver, use `make_tuple`, que copia.

### `apply` e `make_from_tuple`

```cpp
std::apply(calcular, args);              // desempacota a tuple nos parâmetros da função
std::make_from_tuple<Retangulo>(dims);   // o mesmo, para um construtor
```

É a base de qualquer wrapper que guarda argumentos agora e chama depois: `std::bind`, pools
de threads, sistemas de tarefas.

### Percorrer uma tuple

Não existe `for (auto x : tuple)` — cada elemento tem um **tipo** diferente, então o corpo
do laço precisaria ser um template instanciado por elemento. É exatamente o que uma
[fold expression](../templates/) faz, desdobrando em compilação:

```cpp
std::apply([&f](const auto&... e) { (f(e), ...); }, t);
```

Leia `(f(e), ...)` como "repita `f(e)` para cada elemento do pacote, separando por vírgula".

### tuple ou struct?

| Use tuple/pair quando… | Use struct quando… |
|---|---|
| os campos não têm nome natural | o agregado é do **domínio** |
| é interno e efêmero | atravessa uma API pública |
| precisa de `tie`/`apply` | tem invariantes a manter |
| é código genérico sobre N tipos | tem 3+ campos ou métodos próprios |

Medido: `sizeof(tuple<bool,size_t,double>) = 24` e `sizeof(ResultadoDaBusca) = 24`. O custo
é **idêntico** — a escolha é 100% de legibilidade. E desde o C++20 a inicialização designada
(`{.encontrou = true, .indice = 3}`) tirou o último argumento a favor da tuple em código de
aplicação.

---

## Demo 5: `std::span`

### O exemplo mínimo

```cpp
#include <cstdio>
#include <numeric>
#include <span>
#include <vector>

int soma(std::span<const int> v) {                  // UM parâmetro, não dois
    return std::accumulate(v.begin(), v.end(), 0);
}

int main() {
    int         bruto[3]{1, 2, 3};
    std::vector vec{10, 20, 30};

    std::printf("array C : %d\n", soma(bruto));     // o tamanho vem do objeto
    std::printf("vector  : %d\n", soma(vec));
    std::printf("janela  : %d\n", soma(std::span{vec}.first(2)));
}
```

```
array C : 6
vector  : 60
janela  : 30
```

### O problema que resolve

```cpp
void soma_antiga(const int* dados, size_t n);   // ✗ soma_antiga(bruto, 500) compila
int  soma_span(std::span<const int> dados);     // ✓ o tamanho vem do objeto
```

Na primeira, os dois parâmetros precisam concordar e ninguém verifica. Na segunda, o tamanho
é carregado junto com o ponteiro. E como `span` **é um range**, o corpo da função ganha
`for (auto x : dados)` e os algoritmos de `<ranges>` de graça.

### Extent dinâmico vs estático

```
sizeof(span<int>)    = 16      ponteiro + tamanho
sizeof(span<int, 3>) =  8      só o ponteiro — o 3 está no TIPO
sizeof(vector<int>)  = 24      ponteiro + tamanho + capacidade
```

Com extent estático, `v.size()` é constante de compilação e `static_assert(v.extent == 3)`
compila.

A conversão `span<T,3>` → `span<T>` é **implícita** e segura: só se perde informação.

> ⚠ O caminho contrário, `span<T>` → `span<T,N>` via `.first<N>()`, **não é verificado por
> ninguém**. Nem o compilador (o tamanho da origem é dinâmico) nem a biblioteca (não há
> checagem em runtime). Se o span de origem tiver menos de `N` elementos, o resultado é UB
> puro:
>
> ```cpp
> std::vector<int> v{1, 2};
> std::span<int>   d{v};
> auto e = d.first<3>();      // compila em silêncio
> e[2];                       // heap-buffer-overflow
> ```
>
> Compilado com `-fsanitize=address`, esse trecho aborta com
> `AddressSanitizer: heap-buffer-overflow`. `.first<N>()` é uma **afirmação sua**, não uma
> conversão checada.

### Janelas sem cópia

`first(n)`, `last(n)`, `subspan(i, n)` — nenhuma copia nada; todas devolvem outro span sobre
o mesmo buffer. Escrever pela janela muda o vetor original: `span` é uma **visão mutável**.

> ⚠ `subspan` fora dos limites também é **UB**, não exceção. As versões com template
> (`first<4>()`, `subspan<2, 4>()`) checam em compilação **quando o extent de origem é
> estático** — nesse caso, e só nesse, você ganha a verificação.

### `span<const T>` vs `const span<T>`

| Quer proteger… | Escreva |
|---|---|
| os **elementos** | `span<const T>` ← 95% dos casos |
| a **janela** (o ponteiro e o tamanho) | `const span<T>` |
| ambos | `const span<const T>` |

`const std::span<int> ja{v}; ja[0] = 9;` **compila** e altera o vetor: o span é const, os
elementos não. Numa assinatura, `std::span<const T>` já documenta "esta função só lê".

### `borrowed_range` — o que os `<ranges>` exigem

Um range é *borrowed* quando seus **iteradores continuam válidos mesmo depois de o objeto
range ser destruído**, porque ele não possui os dados. Medido pelo demo:

```
span<int>     : true
string_view   : true
vector<int>   : false     ← o objeto POSSUI os dados
vector<int>&  : true      ← a referência não possui nada
```

A consequência é visível: um algoritmo de `<ranges>` que recebe um **temporário** não
borrowed devolve `std::ranges::dangling` em vez de um iterador, e usá-lo não compila:

```cpp
auto it = std::ranges::find(std::vector<int>{1,2,3}, 2);   // compila
*it;   // ✗ erro: no match for 'operator*' (operand type is 'std::ranges::dangling')
```

Com um `span` temporário no lugar do `vector`, o mesmo código compila — porque o span não é
dono de nada e o buffer continua vivo. Ver
[ranges/demo_pipelines.hpp](../ranges/demo_pipelines.hpp).

### ⚠ Tempo de vida

1. `span` como **parâmetro** — o uso dominante, e sempre seguro
2. `span` como **retorno** só se o buffer sobrevive à chamada (ex.: aponta para um membro)
3. `span` como **membro** exige documentar quem é o dono

E a armadilha que o compilador não pega:

```cpp
std::vector<int> v{1, 2, 3};
std::span<int>   s{v};
v.push_back(4);      // realocou → s aponta para memória liberada
s[0] = 1;            // heap-use-after-free
```

Um `span` é tão frágil quanto um par de iteradores — porque **é** isso.

---

## Regras de ouro

| Situação | Use |
|---|---|
| Função pode não achar nada | `optional<T>` |
| …e o **motivo** importa | `expected<T,E>` ([errors/](../errors/)) |
| "Um destes N tipos", lista fechada | `variant<Ts...>` + `visit` |
| Precisa tratar cada tipo do variant | overload idiom |
| O variant precisa de um estado "nada ainda" | `monostate` como primeiro alternativo |
| Tipos definidos por quem **usa** a API | `any` |
| Encontrou cascata de `any_cast` | era um `variant` |
| Dois valores sem nome natural | `pair` |
| 3+ campos, ou API pública | `struct` nomeado |
| Comparação por múltiplos critérios | `std::tie` |
| Parâmetro que recebe sequência contígua | `span<const T>` |
| Retorno que aponta para dentro | só se o dono sobrevive |

---

## Como validar

**1. Rodar o programa inteiro.**

```bash
make build && ./dist/bin/vocabulary_types
```

O que conferir na saída, seção por seção:

| Seção | O que precisa aparecer |
|---|---|
| 1.2 | `&optional` e `&*optional` com **o mesmo endereço** (prova de que não há heap) |
| 1.5 | a linha `false` com `if(o) = true` — a armadilha do `optional<bool>` |
| 2.2 | as quatro formas imprimindo `'texto'`, inclusive `get<1>` |
| 2.5 | `depois: index=-1 valueless=true` |
| 3.2 | `any<Pequeno> → 0 alocação(ões)` e `any<Medio> → 1 alocação(ões)` |
| 3.2 | `variant<...Grande> → 0 alocação(ões)` |
| 5.5 | `vector<int> : false` e `vector<int>& : true` |
| fim | `✓ Todos os demos de tipos de vocabulário concluídos!` |

**2. Sob sanitizadores.** Este subprojeto mexe com tempo de vida e substitui o
`operator new` global; vale conferir que nada disso esconde um defeito:

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/vocabulary_types/main.cpp -o /tmp/vt_san -L$FMTP/lib -lfmtd && /tmp/vt_san
```

Esperado: a mesma saída, **sem nenhum relatório do ASan/UBSan**. Em particular, a seção 5.6
usa spans que apontam para membros vivos — se algum dia aparecer `heap-use-after-free` ali,
o demo quebrou.

**3. Sem warnings.**

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -Wall -Wextra -Wpedantic -I$FMTP/include \
    -c src/vocabulary_types/main.cpp -o /dev/null
```

Esperado: saída vazia.

**4. Os exemplos mínimos deste README** compilam sozinhos, sem fmt e sem o repositório:
`g++ -std=c++23 -Wall -Wextra t.cpp -o t && ./t`.

---

## Referências

Dentro do repositório:

- [`src/errors/`](../errors/) — `std::expected`, o "optional que diz o porquê"
- [`src/ranges/`](../ranges/) — o que é um range, e as views; [demo_pipelines.hpp](../ranges/demo_pipelines.hpp) mostra a proteção contra dangling
- [`src/value_semantics/`](../value_semantics/) — Rule of 5, move `noexcept` e cópia
- [`src/undefined_behavior/`](../undefined_behavior/) — o que UB é, e o que o compilador faz com ele
- [`src/templates/`](../templates/) — pacotes variádicos e fold expressions
- [`src/type_erasure/`](../type_erasure/) — como `any` e `function` são implementados por dentro
- [`src/stl/`](../stl/) — invalidação de iteradores, que é a mesma armadilha do span

Fora do repositório:

- cppreference: [`std::optional`](https://en.cppreference.com/w/cpp/utility/optional) · [`std::variant`](https://en.cppreference.com/w/cpp/utility/variant) · [`std::any`](https://en.cppreference.com/w/cpp/utility/any) · [`std::tuple`](https://en.cppreference.com/w/cpp/utility/tuple) · [`std::span`](https://en.cppreference.com/w/cpp/container/span)
- **P0088** — *Variant: a type-safe union for C++17*
- **P0122** — *span: bounds-safe views for sequences of objects*
- **P0798** — *Monadic operations for std::optional* (as `and_then`/`transform`/`or_else` do C++23)
- **P0637 / P0961** — as regras de *structured bindings*
