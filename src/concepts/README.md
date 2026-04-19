# concepts — C++20 Concepts: Constraints & Requirements

## O que é este projeto?

Demonstra o sistema de **constraints** do C++20, que permite definir requisitos precisos sobre tipos em templates. Concepts substituem SFINAE (Substitution Failure Is Not An Error) com uma sintaxe muito mais legível e mensagens de erro muito melhores.

---

## Por que Concepts?

### Antes (C++17 com SFINAE):
```cpp
// Difícil de ler, mensagens de erro horríveis
template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
soma(T a, T b) { return a + b; }
```

### Depois (C++20 com Concepts):
```cpp
// Claro, legível, erros compreensíveis
template<std::integral T>
T soma(T a, T b) { return a + b; }
```

---

## Estrutura de arquivos

```
concepts/
├── main.cpp              ← executa todos os demos em sequência
├── demo_basics.hpp       ← definição de concepts, std concepts
├── demo_requires.hpp     ← tipos de requires expressions
├── demo_constraints.hpp  ← subsumption, classes com constraints
└── demo_real_world.hpp   ← casos reais: algoritmos genéricos
```

---

## Demo 1: Fundamentos

### Definindo um concept

```cpp
// Um concept é um predicado booleano avaliado em compile-time
template<typename T>
concept Numerico = std::is_arithmetic_v<T>;

template<typename T>
concept Imprimivel = requires(T t) {
    { std::cout << t } -> std::same_as<std::ostream&>;
};

// Usando o concept
template<Numerico T>
T dobro(T x) { return x * 2; }

dobro(42);      // ✓ int é aritmético
dobro(3.14);    // ✓ double é aritmético
dobro("hello"); // ✗ ERRO de compilação: "hello" não satisfaz Numerico
```

### Concepts da stdlib (C++20)

```cpp
// Todos definidos em <concepts>
std::integral<T>        // int, char, bool, etc.
std::floating_point<T>  // float, double, long double
std::signed_integral<T>
std::unsigned_integral<T>
std::arithmetic<T>      // integral || floating_point
std::same_as<T, U>      // T == U
std::derived_from<D, B> // D herda de B
std::convertible_to<T, U>

// Em <iterator>
std::input_iterator<I>
std::forward_iterator<I>
std::random_access_iterator<I>
std::ranges::range<R>
```

---

## Demo 2: Requires Expressions

Existem quatro formas de `requires`:

### 1. Simple requirement — verifica se a expressão é válida

```cpp
template<typename T>
concept Incrementavel = requires(T x) {
    ++x;        // deve compilar — não verifica o tipo do resultado
    x++;
};
```

### 2. Type requirement — verifica se o tipo existe

```cpp
template<typename T>
concept TemIterador = requires {
    typename T::iterator;           // T::iterator deve existir
    typename T::value_type;
};
```

### 3. Compound requirement — verifica expressão + tipo do resultado

```cpp
template<typename T>
concept Somavel = requires(T a, T b) {
    { a + b } -> std::same_as<T>;         // a+b deve existir e retornar T
    { a + b } -> std::convertible_to<int>; // ou conversível para int
};
```

### 4. Nested requirement — requires dentro de requires

```cpp
template<typename T>
concept Container = requires(T c) {
    { c.size() } -> std::convertible_to<std::size_t>;
    { c.begin() } -> std::input_iterator;
    requires std::default_initializable<T>;  // nested requirement
};
```

---

## Demo 3: Constraints Avançadas

### Subsumption — seleção automática da especialização mais específica

```cpp
template<typename T>
concept Numerico = std::is_arithmetic_v<T>;

template<typename T>
concept NumericoInteiro = Numerico<T> && std::integral<T>;
// NumericoInteiro "subsume" Numerico (é mais específico)

template<Numerico T>
void processar(T x) { fmt::print("genérico: {}\n", x); }

template<NumericoInteiro T>
void processar(T x) { fmt::print("inteiro: {}\n", x); }

processar(3.14); // chama versão genérica (double satisfaz Numerico mas não NumericoInteiro)
processar(42);   // chama versão inteiro (int satisfaz ambos, mas NumericoInteiro é mais específico)
```

### Concepts em classes

```cpp
template<std::integral T>
class Contador {
    T valor_ = 0;
public:
    void incrementar() { ++valor_; }
    T get() const { return valor_; }
};

Contador<int>    c1;  // ✓
Contador<double> c2;  // ✗ ERRO: double não é integral
```

---

## Demo 4: Casos Reais

### Algoritmo genérico com contrato explícito

```cpp
template<std::ranges::input_range Range,
         typename Pred>
    requires std::predicate<Pred, std::ranges::range_value_t<Range>>
auto filtrar(const Range& range, Pred pred) {
    std::vector<std::ranges::range_value_t<Range>> result;
    for (auto& e : range)
        if (pred(e)) result.push_back(e);
    return result;
}

std::vector<int> nums = {1, 2, 3, 4, 5, 6};
auto pares = filtrar(nums, [](int x){ return x % 2 == 0; });
```

### Policy-based design com concepts

```cpp
template<typename Logger>
concept LoggerPolicy = requires(Logger l, std::string_view msg) {
    l.log(msg);
    { l.level() } -> std::convertible_to<int>;
};

template<LoggerPolicy L>
class Servidor {
    L logger_;
public:
    void handle(std::string_view req) {
        logger_.log("request: " + std::string(req));
    }
};
```

---

## Formas sintáticas equivalentes

Todas estas formas fazem a mesma coisa:

```cpp
// 1. Usando concept no lugar do typename
template<std::integral T>
T foo(T x);

// 2. Requires clause após a lista de parâmetros do template
template<typename T>
    requires std::integral<T>
T foo(T x);

// 3. Requires clause na assinatura da função
template<typename T>
T foo(T x) requires std::integral<T>;

// 4. Abbreviated function template (mais conciso)
auto foo(std::integral auto x);
```

---

## Vantagens sobre SFINAE

| Aspecto | SFINAE | Concepts |
|---|---|---|
| Legibilidade | Ruim (aninhamentos, `enable_if`) | Excelente |
| Mensagens de erro | Gigantescas, incompreensíveis | Claras e precisas |
| Subsumption | Manual e verboso | Automático |
| Verificação | Implícita via SFINAE | Explícita via `requires` |
| Reutilização | Difícil | Trivial (concepts são nomeados) |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja concepts
./bin/concepts
```

**Requer C++20** (flag `-std=c++20` configurada no meson.build).
