# templates — Metaprogramação de Templates em C++

## O que é este projeto?

Guia progressivo de **templates em C++**: desde o básico (funções e classes genéricas) até técnicas avançadas (SFINAE, variadic templates, CRTP, type traits). Templates são o mecanismo central de genericidade em C++.

---

## Estrutura de arquivos

```
templates/
├── main.cpp                 ← executa todos os demos em sequência
├── demo_basics.hpp          ← funções e classes templates
├── demo_specialization.hpp  ← especialização total e parcial
├── demo_variadic.hpp        ← parameter packs, fold expressions
├── demo_sfinae.hpp          ← SFINAE e enable_if
├── demo_type_traits.hpp     ← type_traits e if constexpr
└── demo_crtp.hpp            ← Curiously Recurring Template Pattern
```

---

## Demo 1: Fundamentos

### Function Templates

```cpp
// Template de função: T é deduzido pelo compilador
template<typename T>
T max(T a, T b) {
    return (a > b) ? a : b;
}

max(3, 7);          // T = int  (deduzido)
max(3.14, 2.71);    // T = double (deduzido)
max<std::string>("abc", "xyz"); // T explícito
```

### Class Templates

```cpp
template<typename T>
class Stack {
    std::vector<T> data_;

public:
    void push(const T& val) { data_.push_back(val); }
    T    pop()               { auto v = data_.back(); data_.pop_back(); return v; }
    bool empty() const       { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
};

Stack<int>         int_stack;
Stack<std::string> str_stack;
// Cada instanciação gera código separado e otimizado para o tipo
```

### Template com múltiplos parâmetros

```cpp
template<typename Key, typename Value>
class Mapa {
    std::vector<std::pair<Key, Value>> entries_;
public:
    void insert(const Key& k, const Value& v) { /* ... */ }
    Value* find(const Key& k);
};

Mapa<std::string, int> contagem;
contagem.insert("hello", 1);
```

---

## Demo 2: Especialização

### Especialização total — comportamento diferente para um tipo específico

```cpp
// Template genérico
template<typename T>
struct Imprimir {
    static void run(T val) { fmt::print("{}\n", val); }
};

// Especialização para bool — exibe "verdadeiro"/"falso"
template<>
struct Imprimir<bool> {
    static void run(bool val) {
        fmt::print("{}\n", val ? "verdadeiro" : "falso");
    }
};

Imprimir<int>::run(42);    // genérico: "42"
Imprimir<bool>::run(true); // especializado: "verdadeiro"
```

### Especialização parcial — para uma família de tipos

```cpp
// Template genérico
template<typename T>
struct Tamanho {
    static std::size_t get() { return sizeof(T); }
};

// Especialização parcial para qualquer ponteiro T*
template<typename T>
struct Tamanho<T*> {
    static std::size_t get() { return sizeof(void*); } // tamanho de ponteiro
};

Tamanho<int>::get();   // sizeof(int) = 4
Tamanho<int*>::get();  // sizeof(void*) = 8 (especialização parcial usada)
```

---

## Demo 3: Variadic Templates & Parameter Packs

### Parameter packs — número variável de argumentos

```cpp
// Sem variadic templates: precisaria de sobrecargas para 1, 2, 3... args
// Com variadic templates: um template para qualquer número

template<typename... Args>
void imprimir_tudo(Args&&... args) {
    // Fold expression (C++17): aplica operação a todos os elementos do pack
    (fmt::print("{} ", std::forward<Args>(args)), ...);
    fmt::print("\n");
}

imprimir_tudo(1, "hello", 3.14, true);  // "1 hello 3.14 true"
```

### Recursão com parameter packs (C++11/14)

```cpp
// Caso base
void imprimir() {}

// Caso recursivo: processa um argumento, recursa com o resto
template<typename First, typename... Rest>
void imprimir(First first, Rest... rest) {
    fmt::print("{} ", first);
    imprimir(rest...);  // recursa com pack menor
}
```

### Fold expressions (C++17)

```cpp
// Soma de qualquer número de valores
template<typename... Ts>
auto soma(Ts... vals) {
    return (vals + ...); // fold: v1 + v2 + v3 + ...
}

soma(1, 2, 3, 4, 5); // 15

// Verificar se todos satisfazem condição
template<typename... Ts>
bool todos_positivos(Ts... vals) {
    return ((vals > 0) && ...); // fold com &&
}
```

---

## Demo 4: SFINAE — Substitution Failure Is Not An Error

Quando o compilador tenta instanciar um template e a substituição falha (ex: o tipo não tem o método pedido), **não é um erro** — o compilador simplesmente descarta essa sobrecarga e tenta a próxima.

```cpp
// Habilitar função apenas para tipos integrais
template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
dobro(T x) {
    return x * 2;
}

// Habilitar função apenas para tipos de ponto flutuante
template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
dobro(T x) {
    return x * 2.0; // pode fazer algo diferente
}

dobro(42);    // usa versão integral
dobro(3.14);  // usa versão floating_point
dobro("hi");  // ERRO: nenhuma versão se aplica
```

### `enable_if` com parâmetro template padrão

```cpp
// Forma mais comum e limpa
template<typename T,
         typename = std::enable_if_t<std::is_integral_v<T>>>
T proximo(T x) { return x + 1; }
```

### Detecção de membros

```cpp
// Detecta se tipo T tem método .size()
template<typename T, typename = void>
struct tem_size : std::false_type {};

template<typename T>
struct tem_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

static_assert(tem_size<std::vector<int>>::value);
static_assert(!tem_size<int>::value);
```

---

## Demo 5: Type Traits & `if constexpr`

### Type traits — inspeção de tipos em compile-time

```cpp
// Verificações de tipo
std::is_integral_v<int>          // true
std::is_floating_point_v<double> // true
std::is_pointer_v<int*>          // true
std::is_const_v<const int>       // true
std::is_same_v<int, int>         // true
std::is_same_v<int, long>        // false

// Transformações de tipo
std::remove_const_t<const int>   // int
std::add_pointer_t<int>          // int*
std::remove_reference_t<int&>    // int
std::decay_t<int[5]>             // int*
```

### `if constexpr` — branches em compile-time

```cpp
template<typename T>
void processar(T val) {
    if constexpr (std::is_integral_v<T>) {
        fmt::print("inteiro: {} (dobro: {})\n", val, val * 2);
        // Este branch só é COMPILADO quando T for integral
        // (sem if constexpr, o compilador tenta compilar os dois branches)
    } else if constexpr (std::is_floating_point_v<T>) {
        fmt::print("float: {:.4f}\n", val);
    } else {
        fmt::print("outro tipo\n");
    }
}

processar(42);      // branch integral
processar(3.14);    // branch floating_point
processar("hello"); // branch else
```

---

## Demo 6: CRTP — Curiously Recurring Template Pattern

CRTP é um idioma onde uma classe herda de um template parametrizado com ela mesma. Permite **polimorfismo estático** (compile-time) sem o custo de `virtual`.

```cpp
// Interface base via CRTP
template<typename Derived>
class Animal {
public:
    // Chama método da subclasse SEM virtual — resolvido em compile-time
    void falar() const {
        static_cast<const Derived*>(this)->falar_impl();
    }

    // Método com comportamento padrão reutilizável
    void apresentar() const {
        fmt::print("Sou um ");
        falar(); // polimorfismo estático
    }
};

class Cachorro : public Animal<Cachorro> {
public:
    void falar_impl() const { fmt::print("Au!\n"); }
};

class Gato : public Animal<Gato> {
public:
    void falar_impl() const { fmt::print("Miau!\n"); }
};

Cachorro c;
c.apresentar(); // "Sou um Au!" — sem vtable, sem overhead
```

### CRTP vs Virtual

| Aspecto | `virtual` | CRTP |
|---|---|---|
| Resolução | Runtime | Compile-time |
| Overhead | vtable + vptr | Zero |
| Containers polimórficos | `vector<Animal*>` | Não (tipos diferentes) |
| Código gerado | Um por hierarquia | Um por tipo |
| Debugging | Mais fácil | Mensagens de erro verbosas |

### CRTP para mixin de funcionalidades

```cpp
// Adiciona operadores de comparação dado que a classe fornece operator<
template<typename Derived>
class Comparavel {
public:
    bool operator>(const Derived& o) const { return o < *static_cast<const Derived*>(this); }
    bool operator<=(const Derived& o) const { return !(o < *static_cast<const Derived*>(this)); }
    bool operator>=(const Derived& o) const { return !(*static_cast<const Derived*>(this) < o); }
};

class Temperatura : public Comparavel<Temperatura> {
    double graus_;
public:
    explicit Temperatura(double g) : graus_(g) {}
    bool operator<(const Temperatura& o) const { return graus_ < o.graus_; }
};

Temperatura t1(20.0), t2(25.0);
bool b1 = t1 < t2;  // operator< definido
bool b2 = t1 > t2;  // operado via CRTP Comparavel
bool b3 = t1 <= t2; // idem
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja templates
./bin/templates
```

**Requer C++17** para fold expressions e `if constexpr`.
