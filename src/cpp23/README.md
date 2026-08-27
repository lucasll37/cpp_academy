# cpp23 — O C++23 Neste Toolchain

## O que é este projeto?

Diferente dos outros: além de **ensinar** as features do C++23, ele **mede** quais estão
disponíveis, usando feature-test macros.

A tabela de suporte em [src/ROADMAP.md](../ROADMAP.md) é gerada a partir do que este
programa imprime — e teve **seis entradas corrigidas** quando a medição substituiu a
suposição.

---

## Por que estudar isto?

Porque "C++23" não é algo que um compilador tem ou não tem. Cada feature chega numa versão
diferente, e a biblioteca costuma atrasar em relação à linguagem.

O que eu presumia vs o que o GCC 13.3 **realmente** tem:

| Feature | Eu presumia | Medido |
|---|---|---|
| `std::stacktrace` | ausente | **presente** (com `-lstdc++_libbacktrace`) |
| `views::zip` | ausente | **presente** |
| `ranges::fold_left` | ausente | **presente** |
| `views::chunk` / `slide` | ausente | **presente** |
| `std::move_only_function` | ausente | **presente** |
| `std::spanstream` | ausente | **presente** |

Todas na direção de **subestimar** o que já estava disponível.

---

## Estrutura de arquivos

```
cpp23/
├── main.cpp                 ← executa todos os demos
├── demo_inventario.hpp      ← feature-test macros e o inventário medido
├── demo_linguagem.hpp       ← if consteval, subscript multidim, uz, [[assume]]
├── demo_biblioteca.hpp      ← to_underlying, unreachable, zip, monádicas
├── demo_disponivel.hpp      ← move_only_function, spanstream, chunk/slide
└── demo_o_que_falta.hpp     ← o que falta, e o substituto de cada coisa
```

---

## Demo 1: Feature-Test Macros

```cpp
#ifdef __cpp_lib_expected
  // a data de adoção: 202211 = novembro de 2022
#endif
```

| Prefixo | Origem |
|---|---|
| `__cpp_*` | predefinidas pelo **compilador** (linguagem) |
| `__cpp_lib_*` | definidas em `<version>` (biblioteca) |

O valor permite testar **versões** da mesma feature:
`#if __cpp_lib_optional >= 202110` (as monádicas).

### O inventário medido

**Linguagem:**

```
if consteval                    ✓ 202106
operator[](i, j, k)             ✓ 202211
static operator()               ✓ 202207
sufixo uz / z                   ✓ 202011
\N{NOME DO CARACTERE}           ✓ 202207
deducing this                   ✗ ausente     ← a mais sentida
```

**Biblioteca — presentes:** `expected` · `byteswap` · `to_underlying` · `unreachable` ·
`views::zip` · `string::contains` · `stacktrace`\*

**Biblioteca — ausentes:** `print`/`println` · `mdspan` · `generator` · `ranges::to<C>()` ·
`flat_map` · `views::enumerate` · `ranges::starts_with` · `out_ptr`

> \* ⚠ **A macro está definida e o link falha.** `std::stacktrace` precisa de
> `-lstdc++_libbacktrace`; sem ela:
> `undefined reference to '__glibcxx_backtrace_simple'`.
>
> **A macro diz que a API existe, não que ela linka.** Esta é a lição mais útil do
> subprojeto.

### Degradar com elegância

```cpp
#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
    return std::byteswap(v);
#elif defined(__GNUC__)
    return __builtin_bswap32(v);
#else
    return /* shifts portáveis */;
#endif
```

Três regras: teste a **macro**, não a versão do compilador · compare com o **valor**, não só
`#ifdef` · o fallback precisa ser **equivalente** (o `static_assert` verifica).

---

## Demo 2: Linguagem

### `if consteval`

Por que `is_constant_evaluated()` não bastava:

```cpp
constexpr int f(int x) {
    if (std::is_constant_evaluated())
        return so_em_compilacao(x);   // ✗ NÃO COMPILA
    return x * 2;
}
```

O ramo de um `if` normal **não é contexto imediato**, então chamar uma `consteval` ali é
erro. O `if consteval` **cria** esse contexto.

### ⚠ Subscript multidimensional — a armadilha de compatibilidade

Em C++20, `a[i, j]` era o operador **vírgula** (avalia `i`, descarta, indexa por `j`). O
C++23 reciclou a sintaxe — código antigo **muda de comportamento** ao recompilar.
Use `-Wcomma-subscript`.

### Os sufixos `uz` e `z`

```cpp
for (auto i = 0uz; i < v.size(); ++i)   // sem -Wsign-compare, sem static_cast
```

`0uz` é `size_t`, `0z` é `ptrdiff_t`.

### `[[assume]]`

```cpp
[[assume(expoente >= 0 && expoente < 31)]];
```

| | Verifica | Se falso |
|---|---|---|
| `assert(x > 0)` | sim | aborta |
| `[[assume(x > 0)]]` | **não** | **UB** |

A expressão **não é avaliada** — `[[assume(++i > 0)]]` não incrementa `i`.

> Use com parcimônia, e sempre com um `assert` em debug: assim o build de debug **prova** o
> que o de release **assume**.

---

## Demo 3: Biblioteca

### `std::to_underlying`

```cpp
static_cast<std::underlying_type_t<Codigo>>(c)   // antes
std::to_underlying(c)                             // agora
```

O ganho não é digitar menos — é **não repetir o tipo**. Se `Codigo` mudar de `uint16_t` para
`uint32_t`, o `static_cast` explícito continua compilando e **trunca**.

### `std::unreachable`

Um `switch` que cobre todos os enumeradores ainda gera *"control reaches end of non-void
function"* — porque um `enum class` **pode** guardar valor fora da lista.

| Saída | Veredito |
|---|---|
| `return {};` | esconde o bug com um valor falso |
| `throw ...;` | custa, e o caso é impossível |
| `std::unreachable();` | **diz a verdade ao compilador** |

`unreachable()` ≡ `[[assume(false)]]`. Os dois são UB se a premissa estiver errada.

### `views::zip`

```cpp
for (const auto& [nome, idade, salario] : std::views::zip(nomes, idades, salarios))
```

Para no **menor** range. E o substituto para o `enumerate` ausente:

```cpp
std::views::zip(std::views::iota(0uz), v)
```

---

## Demo 4: Presentes e Pouco Usadas

### `std::move_only_function`

`std::function` exige que o chamável seja **copiável** — mesmo quando ninguém vai copiá-lo:

```cpp
std::function<std::string()> f = [r = std::move(ptr)] { ... };            // ✗
std::move_only_function<std::string()> f = [r = std::move(ptr)] { ... };  // ✓
```

Onde morde: fila de tarefas de thread pool (a tarefa captura um `promise`, que é move-only).

E ele qualifica a chamada: `void() const`, `void() &&`, `void() noexcept` — coisas que
`std::function` nunca teve.

### `std::spanstream`

Stream sobre um buffer que **já existe**, sem alocar:

```cpp
char dados[] = "42 3.14 texto";
std::ispanstream entrada{std::span<char>{dados, std::size(dados) - 1}};
```

> ⚠ O `- 1` não é detalhe: `std::span{dados}` sobre um `char[]` inclui o `'\0'`, e o
> terminador vira um caractere de dado.

Substitui o `strstream` depreciado, que não deixava claro quem era dono do buffer. O `span`
resolve por construção: ele **nunca** é dono.

### `views::chunk` e `slide`

```
chunk(3): [1 2 3] [4 5 6] [7 8 9] [10]        ← disjuntos, o último pode ser menor
slide(3): [1 2 3] [2 3 4] [3 4 5] ...          ← deslizante, todos do mesmo tamanho
```

Média móvel em uma linha: `v | views::slide(3)` + `ranges::fold_left`.

---

## Demo 5: O Que Falta

Cada ausência vem com um substituto que **funciona hoje**:

| Ausente | Substituto |
|---|---|
| `std::print` / `println` | **`fmt::print` / `fmt::println`** — API idêntica |
| `std::mdspan` | `operator[](i,j)` próprio; ou `kokkos/mdspan` |
| `ranges::to<C>()` | `std::vector v(view.begin(), view.end())` |
| `views::enumerate` | `views::zip(views::iota(0uz), v)` |
| `std::generator` | `promise_type` à mão ([coroutines/](../coroutines/)) |
| deducing this | y-combinator ([lambdas/](../lambdas/)) · CRTP |
| `std::flat_map` | vector ordenado ([compile_time/](../compile_time/)) |
| `ranges::starts_with` | `std::equal` |

> `std::print` **nasceu** da `fmt` — a proposta P2093 foi escrita pelo autor da biblioteca.
> É por isso que este repositório usa `fmt` em todos os subprojetos: a migração será trocar
> `fmt::` por `std::`.

### Quando adotar uma feature nova

A pergunta não é *"o C++23 tem?"*, é *"o meu toolchain **mínimo** tem?"*:

1. Qual é o compilador mais antigo que precisa compilar isto?
2. A feature-test macro está definida nele?
3. Ela **linka** sem flags extras?

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Saber se uma feature existe | feature-test macro, **medida** |
| …e se ela linka | compile de verdade — a macro não basta |
| Código que roda em vários toolchains | `#if` na macro + fallback equivalente |
| Converter `enum class` para o tipo base | `std::to_underlying` |
| `switch` exaustivo sobre enum | `std::unreachable()` no fim |
| Prometer algo ao otimizador | `[[assume]]` + `assert` em debug |
| Percorrer sequências em paralelo | `views::zip` |
| Índice + valor | `views::zip(views::iota(0uz), v)` |
| Guardar chamável move-only | `std::move_only_function` |
| Parsear buffer existente | `std::ispanstream` |
| Formatar | `fmt::print` hoje, `std::print` amanhã |

---

## Como compilar e executar

```bash
make build
./dist/bin/cpp23
```

Para reproduzir o inventário em outro toolchain:

```bash
echo '#include <version>' > /tmp/v.cpp
g++ -std=c++23 -dM -E /tmp/v.cpp | grep __cpp_lib | sort
```

Saída esperada ao final: `✓ Todos os demos de C++23 concluídos!`
