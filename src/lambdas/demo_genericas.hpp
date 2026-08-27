// =============================================================================
//  src/lambdas/demo_genericas.hpp
//
//  Lambdas genéricas — templates sem escrever `template`
//  ─────────────────────────────────────────────────────
//  Desde o C++14, um parâmetro `auto` numa lambda gera um `operator()` que é
//  um TEMPLATE. A classe anônima passa a ter um método template:
//
//      auto f = [](auto x) { return x * 2; };
//
//      // vira:
//      class __lambda {
//      public:
//          template <typename T>
//          auto operator()(T x) const { return x * 2; }
//      };
//
//  O C++20 acrescentou a sintaxe explícita `[]<typename T>(...)`, que devolve
//  o acesso ao parâmetro de tipo — necessário para perfect forwarding correto,
//  para restringir com concepts, e para trabalhar com pacotes variádicos.
//
//  Tópicos:
//  • `auto` como parâmetro e o operator() template
//  • Lambdas com template explícito (C++20)
//  • Perfect forwarding numa lambda
//  • Lambdas variádicas e fold expressions
//  • Restringindo com concepts
//  • Lambdas em contexto constexpr
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <array>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo_genericas {

// ─────────────────────────────────────────────────────────────────────────────
//  1. `auto` como parâmetro
// ─────────────────────────────────────────────────────────────────────────────
void demo_auto_param() {
    fmt::print("\n── 2.1 `auto` como parâmetro ──\n");

    auto dobrar = [](auto x) { return x * 2; };

    fmt::print("      dobrar(21)      = {}\n",     dobrar(21));
    fmt::print("      dobrar(1.5)     = {}\n",     dobrar(1.5));
    fmt::print("      dobrar('A')     = {}\n",     dobrar('A'));      // char → int

    // Uma lambda genérica é UM objeto que gera N instanciações
    auto descrever = [](const auto& x) {
        if constexpr (std::is_integral_v<std::decay_t<decltype(x)>>) {
            return fmt::format("inteiro {}", x);
        } else if constexpr (std::is_floating_point_v<std::decay_t<decltype(x)>>) {
            return fmt::format("real {:.2f}", x);
        } else {
            return fmt::format("outro: {}", x);
        }
    };

    fmt::print("      descrever(42)     → {}\n", descrever(42));
    fmt::print("      descrever(3.14)   → {}\n", descrever(3.14));
    fmt::print("      descrever(\"texto\")→ {}\n", descrever("texto"));

    fmt::print("\n      Um único OBJETO, três instanciações do operator().\n");
    fmt::print("      `if constexpr` dentro dela poda os ramos que não se aplicam\n");
    fmt::print("      a cada tipo — o código do ramo descartado nem é compilado.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Template explícito (C++20) — quando o `auto` não basta
//
//     Com `auto`, você não tem NOME para o tipo. Isso impede:
//       • `std::forward<T>` correto
//       • usar `T` para declarar variáveis ou restringir
//       • acessar `T::algo`
// ─────────────────────────────────────────────────────────────────────────────
void demo_template_explicito() {
    fmt::print("\n── 2.2 Template explícito (C++20) ──\n");

    // Com auto: para forwardar, é preciso o decltype completo — verboso e frágil
    auto com_auto = [](auto&& x) {
        return std::forward<decltype(x)>(x);
    };

    // Com template explícito: o T está disponível, e fica legível
    auto com_template = []<typename T>(T&& x) {
        return std::forward<T>(x);
    };

    std::string s = "valor";
    fmt::print("      com_auto(s)     → '{}'\n",     com_auto(s));
    fmt::print("      com_template(s) → '{}'\n",     com_template(s));

    // O ganho real: usar T para MAIS do que forwardar
    auto info = []<typename T>(const T&) {
        return fmt::format("sizeof={} trivial={}",
                           sizeof(T), std::is_trivially_copyable_v<T>);
    };
    fmt::print("      info(42)        → {}\n", info(42));
    fmt::print("      info(std::string) → {}\n", info(std::string{}));

    // E acessar tipos aninhados, o que `auto` sozinho não permite
    auto primeiro_elemento = []<typename C>(const C& c) -> typename C::value_type {
        return c.empty() ? typename C::value_type{} : c.front();
    };
    fmt::print("      primeiro_elemento(vector{{7,8,9}}) = {}\n",
               primeiro_elemento(std::vector<int>{7, 8, 9}));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Perfect forwarding numa lambda
//
//     A regra é a mesma de src/value_semantics/demo_perfect_forwarding.hpp:
//     `auto&&` é referência universal, e o repasse exige `std::forward`.
// ─────────────────────────────────────────────────────────────────────────────
struct Rastreado {
    std::string nome;
    Rastreado(std::string n) : nome(std::move(n)) {}
    Rastreado(const Rastreado& o) : nome(o.nome)            { ++copias; }
    Rastreado(Rastreado&& o) noexcept : nome(std::move(o.nome)) { ++moves; }
    static inline int copias = 0, moves = 0;
    static void zerar() { copias = moves = 0; }
};

void consumir(Rastreado) {}

void demo_forwarding() {
    fmt::print("\n── 2.3 Perfect forwarding ──\n");

    // ✗ Sem forward: o parâmetro tem NOME, logo é lvalue → sempre copia
    auto sem_forward = [](auto&& x) { consumir(x); };

    // ✓ Com forward: preserva a categoria de valor original
    auto com_forward = []<typename T>(T&& x) { consumir(std::forward<T>(x)); };

    Rastreado::zerar();
    sem_forward(Rastreado{"a"});
    fmt::print("      sem forward, arg rvalue: {} cópia(s), {} move(s)\n",
               Rastreado::copias, Rastreado::moves);

    Rastreado::zerar();
    com_forward(Rastreado{"b"});
    fmt::print("      com forward, arg rvalue: {} cópia(s), {} move(s)\n",
               Rastreado::copias, Rastreado::moves);

    Rastreado lv{"c"};
    Rastreado::zerar();
    com_forward(lv);
    fmt::print("      com forward, arg lvalue: {} cópia(s), {} move(s)  ← correto\n",
               Rastreado::copias, Rastreado::moves);

    fmt::print("\n      O forward é CONDICIONAL: move se o chamador deu rvalue,\n");
    fmt::print("      copia se deu lvalue. É por isso que ele existe.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Lambdas variádicas e fold expressions
// ─────────────────────────────────────────────────────────────────────────────
void demo_variadicas() {
    fmt::print("\n── 2.4 Lambdas variádicas ──\n");

    // Soma de qualquer número de argumentos, de qualquer tipo somável
    auto somar = [](auto... args) { return (args + ...); };
    fmt::print("      somar(1,2,3,4,5)       = {}\n", somar(1, 2, 3, 4, 5));
    fmt::print("      somar(1.5, 2.5)        = {}\n", somar(1.5, 2.5));

    // Imprimir todos — fold sobre o operador vírgula
    auto imprimir_todos = [](const auto&... args) {
        ((fmt::print("[{}] ", args)), ...);
        fmt::print("\n");
    };
    fmt::print("      imprimir_todos(1,'a',\"txt\",2.5): ");
    imprimir_todos(1, 'a', "txt", 2.5);

    // Contar quantos satisfazem um predicado
    auto contar_pares = [](auto... args) {
        return ((args % 2 == 0 ? 1 : 0) + ...);
    };
    fmt::print("      contar_pares(1,2,3,4,5,6) = {}\n", contar_pares(1, 2, 3, 4, 5, 6));

    // Variádica COM forwarding: o padrão de uma fábrica
    auto criar_string = []<typename... Ts>(Ts&&... args) {
        return std::string(std::forward<Ts>(args)...);
    };
    fmt::print("      criar_string(5, 'x')   = '{}'\n", criar_string(5, 'x'));

    fmt::print("\n      | fold                | expande para              |\n");
    fmt::print("      |---------------------|---------------------------|\n");
    fmt::print("      | (args + ...)        | a1 + (a2 + (a3 + a4))      |\n");
    fmt::print("      | (... + args)        | ((a1 + a2) + a3) + a4      |\n");
    fmt::print("      | (init + ... + args) | com valor inicial          |\n");
    fmt::print("      | ((f(args)), ...)    | f(a1), f(a2), f(a3), ...   |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Restringindo com concepts
//
//     Uma lambda genérica aceita QUALQUER tipo — e falha lá dentro, com erro
//     ilegível, se o tipo não servir. Um concept move o erro para a chamada.
// ─────────────────────────────────────────────────────────────────────────────
void demo_concepts() {
    fmt::print("\n── 2.5 Restringindo com concepts ──\n");

    // (a) Concept no lugar do `auto`
    auto dobrar_numero = [](std::integral auto x) { return x * 2; };
    fmt::print("      [](std::integral auto x) → dobrar_numero(21) = {}\n",
               dobrar_numero(21));
    // dobrar_numero(1.5);          // ✗ não compila: double não é integral
    fmt::print("      dobrar_numero(1.5) → NÃO COMPILA (double não é integral)\n");

    // (b) Cláusula `requires` completa
    auto media = []<typename C>(const C& c) requires std::ranges::range<C> {
        double soma = 0; std::size_t n = 0;
        for (const auto& x : c) { soma += x; ++n; }
        return n ? soma / static_cast<double>(n) : 0.0;
    };
    fmt::print("      media(vector{{1,2,3,4}}) = {:.2f}\n",
               media(std::vector<int>{1, 2, 3, 4}));

    // (c) requires ad-hoc, sem nomear um concept
    auto tem_size = []<typename T>(const T& x) requires requires { x.size(); } {
        return x.size();
    };
    fmt::print("      tem_size(string \"abc\") = {}\n", tem_size(std::string{"abc"}));
    fmt::print("      tem_size(42) → NÃO COMPILA (int não tem .size())\n");

    fmt::print("\n      Sem concept, `dobrar_numero(1.5)` compilaria e devolveria\n");
    fmt::print("      3.0 silenciosamente. Com concept, o erro é na CHAMADA e\n");
    fmt::print("      diz qual constraint falhou (ver src/concepts/).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Lambdas em compilação
//
//     Desde o C++17, lambdas são implicitamente `constexpr` quando possível.
//     Isso as torna utilizáveis dentro de `static_assert` e como inicializador
//     de variáveis constexpr — o idioma IIFE de inicialização complexa.
// ─────────────────────────────────────────────────────────────────────────────
void demo_constexpr() {
    fmt::print("\n── 2.6 Lambdas em compilação ──\n");

    constexpr auto quadrado = [](int x) { return x * x; };
    static_assert(quadrado(7) == 49);
    fmt::print("      static_assert(quadrado(7) == 49) ✓ (lambda constexpr)\n");

    // O idioma IIFE: inicializar um `const` complexo sem função auxiliar
    constexpr auto tabela = [] {
        std::array<int, 10> t{};
        for (std::size_t i = 0; i < t.size(); ++i) {
            t[i] = static_cast<int>(i * i);
        }
        return t;
    }();                              // ← invocada imediatamente

    static_assert(tabela[9] == 81);
    fmt::print("      tabela gerada por IIFE constexpr: ");
    for (int x : tabela) fmt::print("{} ", x);
    fmt::print("\n");

    fmt::print("\n      O IIFE (Immediately Invoked Function Expression) resolve\n");
    fmt::print("      um problema real: inicializar um `const` que precisa de\n");
    fmt::print("      várias instruções, sem poluir o escopo com uma função\n");
    fmt::print("      auxiliar usada uma vez só.\n");
    fmt::print("      Ver src/compile_time/demo_tabelas.hpp para o uso em escala.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_auto_param();
    demo_template_explicito();
    demo_forwarding();
    demo_variadicas();
    demo_concepts();
    demo_constexpr();
}

} // namespace demo_genericas
