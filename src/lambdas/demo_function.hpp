// =============================================================================
//  src/lambdas/demo_function.hpp
//
//  std::function — apagar o tipo, e o que isso custa
//  ─────────────────────────────────────────────────
//  Cada lambda tem um TIPO ÚNICO e anônimo. Isso é ótimo para o compilador
//  (pode inlinar) e péssimo para guardar: não dá para declarar uma variável
//  do tipo de uma lambda sem `auto`, nem colocar duas lambdas diferentes no
//  mesmo `vector`.
//
//  `std::function<R(Args...)>` resolve isso apagando o tipo: ele aceita
//  qualquer chamável com aquela assinatura. O preço é indireção, possível
//  alocação, e o fim do inlining.
//
//  Tópicos:
//  • O problema: lambdas têm tipos distintos
//  • Como o type erasure funciona por dentro (uma implementação mínima)
//  • O custo MEDIDO: template vs function vs ponteiro
//  • SBO: quando `function` aloca e quando não
//  • Quando usar cada um
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace demo_function {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O problema
// ─────────────────────────────────────────────────────────────────────────────
void demo_o_problema() {
    fmt::print("\n── 3.1 O problema: cada lambda é um tipo ──\n");

    auto somar    = [](int a, int b) { return a + b; };
    auto subtrair = [](int a, int b) { return a - b; };

    fmt::print("      mesma assinatura, tipos iguais? {}\n",
               std::is_same_v<decltype(somar), decltype(subtrair)>);

    // std::vector<decltype(somar)> v{somar, subtrair};   // ✗ não compila
    fmt::print("      vector<decltype(somar)>{{somar, subtrair}} → NÃO COMPILA\n");

    // Com std::function, os dois cabem no mesmo container
    const std::vector<std::function<int(int, int)>> operacoes{
        somar,
        subtrair,
        [](int a, int b) { return a * b; },
        std::plus<int>{},                          // functor da STL
    };

    fmt::print("      vector<std::function<int(int,int)>> aceita todos:\n");
    for (const auto& op : operacoes) fmt::print("        op(10, 3) = {}\n", op(10, 3));

    fmt::print("\n      `std::function` aceita QUALQUER chamável com a assinatura:\n");
    fmt::print("        lambda · functor · ponteiro de função · ponteiro para\n");
    fmt::print("        membro · objeto com operator()\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Como o type erasure funciona
//
//     A técnica: uma interface virtual interna, e um template que a implementa
//     para cada tipo concreto. O `function` guarda um ponteiro para a
//     interface — e é por isso que a chamada não pode ser inlined.
// ─────────────────────────────────────────────────────────────────────────────
template <typename Assinatura>
class MinhaFunction;

template <typename R, typename... Args>
class MinhaFunction<R(Args...)> {
public:
    MinhaFunction() = default;

    // Aceita qualquer chamável — e o embrulha num Modelo<F> concreto
    template <typename F>
    MinhaFunction(F f) : impl_(std::make_unique<Modelo<F>>(std::move(f))) {}

    R operator()(Args... args) const {
        // A chamada virtual: o compilador não sabe qual código roda aqui
        return impl_->chamar(std::forward<Args>(args)...);
    }

    explicit operator bool() const { return impl_ != nullptr; }

private:
    // A interface apagada: só a ASSINATURA sobrevive
    struct Conceito {
        virtual ~Conceito() = default;
        virtual R chamar(Args...) const = 0;
    };

    // A implementação concreta, uma por tipo de chamável
    template <typename F>
    struct Modelo final : Conceito {
        explicit Modelo(F f) : f_(std::move(f)) {}
        R chamar(Args... args) const override {
            return f_(std::forward<Args>(args)...);
        }
        mutable F f_;
    };

    std::unique_ptr<Conceito> impl_;
};

void demo_como_funciona() {
    fmt::print("\n── 3.2 Como o type erasure funciona ──\n");

    MinhaFunction<int(int, int)> f = [](int a, int b) { return a + b; };
    fmt::print("      MinhaFunction<int(int,int)> f = lambda;  f(3,4) = {}\n", f(3, 4));

    f = [](int a, int b) { return a * b; };       // troca o alvo
    fmt::print("      após f = outra lambda;                   f(3,4) = {}\n", f(3, 4));

    fmt::print("\n      A estrutura, em 3 partes:\n");
    fmt::print("        struct Conceito {{ virtual R chamar(Args...) = 0; }};\n");
    fmt::print("        template<F> struct Modelo : Conceito {{ F f_; ... }};\n");
    fmt::print("        std::unique_ptr<Conceito> impl_;\n");
    fmt::print("      O tipo concreto `F` some da assinatura de `MinhaFunction`,\n");
    fmt::print("      mas continua vivo dentro do `Modelo<F>`.\n");
    fmt::print("      É o MESMO padrão do `std::any` (src/vocabulary_types/) e\n");
    fmt::print("      do component store do ECS (src/ecs/).\n");

    fmt::print("\n      E é daí que vem o custo: a chamada passa por uma vtable,\n");
    fmt::print("      e o compilador não consegue inlinar o corpo da lambda.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. O custo, medido
// ─────────────────────────────────────────────────────────────────────────────
template <typename F>
long aplicar_template(const std::vector<int>& v, F f) {
    long total = 0;
    for (int x : v) total += f(x);
    return total;
}

long aplicar_function(const std::vector<int>& v, const std::function<int(int)>& f) {
    long total = 0;
    for (int x : v) total += f(x);
    return total;
}

long aplicar_ponteiro(const std::vector<int>& v, int (*f)(int)) {
    long total = 0;
    for (int x : v) total += f(x);
    return total;
}

int dobrar_livre(int x) { return x * 2; }

void demo_custo() {
    fmt::print("\n── 3.3 O custo, medido ──\n");

    std::vector<int> v(2'000'000);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<int>(i % 100);

    auto dobrar = [](int x) { return x * 2; };
    using clock = std::chrono::steady_clock;

    auto cronometrar = [](auto&& fn) {
        const auto t0 = clock::now();
        const volatile long r = fn();
        (void)r;
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   clock::now() - t0).count();
    };

    const auto t_tpl  = cronometrar([&] { return aplicar_template(v, dobrar); });
    const auto t_fn   = cronometrar([&] { return aplicar_function(v, dobrar); });
    const auto t_ptr  = cronometrar([&] { return aplicar_ponteiro(v, dobrar_livre); });

    fmt::print("      {} elementos, dobrando cada um:\n", v.size());
    fmt::print("        template<F>      : {:>5} ms  ← inlinável\n", t_tpl);
    fmt::print("        std::function    : {:>5} ms  ← chamada virtual\n", t_fn);
    fmt::print("        ponteiro p/ função: {:>4} ms  ← chamada indireta\n", t_ptr);

    fmt::print("\n      ⚠  medido em -O0 (build de debug deste repo). Em -O2 o\n");
    fmt::print("         template costuma sumir por completo (o laço vira\n");
    fmt::print("         vetorizado), enquanto `std::function` mantém a chamada\n");
    fmt::print("         indireta. A diferença AUMENTA com otimização.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. SBO — quando `std::function` aloca
//
//     A libstdc++ guarda chamáveis pequenos DENTRO do próprio `function`
//     (small buffer optimization). Acima do limite, vai para a heap.
// ─────────────────────────────────────────────────────────────────────────────
namespace contador {
inline std::size_t alocacoes = 0;
inline bool        ativo     = false;
} // namespace contador

template <typename F>
std::size_t contar_alocacoes(F fabrica) {
    contador::alocacoes = 0;
    contador::ativo = true;
    { auto f = fabrica(); (void)f; }
    contador::ativo = false;
    return contador::alocacoes;
}

void demo_sbo() {
    fmt::print("\n── 3.4 SBO — quando `function` aloca ──\n");

    fmt::print("      sizeof(std::function<void()>) = {}\n",
               sizeof(std::function<void()>));

    const auto a0 = contar_alocacoes([] {
        return std::function<int()>{[] { return 42; }};                  // sem captura
    });
    int x = 1;
    const auto a1 = contar_alocacoes([x] {
        return std::function<int()>{[x] { return x; }};                  // 1 int
    });
    struct Grande { char dados[64]; };
    const Grande g{};
    const auto a2 = contar_alocacoes([g] {
        return std::function<int()>{[g] { return static_cast<int>(g.dados[0]); }};
    });

    fmt::print("      lambda sem captura        → {} alocação(ões)\n", a0);
    fmt::print("      lambda capturando 1 int   → {} alocação(ões)\n", a1);
    fmt::print("      lambda capturando 64 B    → {} alocação(ões)  ← estourou o SBO\n", a2);

    fmt::print("\n      Consequência prática: um `std::function` construído dentro\n");
    fmt::print("      de um loop quente pode fazer malloc/free a cada iteração.\n");
    fmt::print("      Se a captura for grande, construa FORA do laço.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Quando usar cada um
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando() {
    fmt::print("\n── 3.5 Escolhendo a ferramenta ──\n");

    fmt::print("      | ferramenta         | inlina | aloca | guardável | assinatura |\n");
    fmt::print("      |--------------------|--------|-------|-----------|------------|\n");
    fmt::print("      | template<F> f      | SIM    | não   | não       | genérica   |\n");
    fmt::print("      | auto f = lambda    | SIM    | não   | local     | genérica   |\n");
    fmt::print("      | ponteiro p/ função | não    | não   | sim       | fixa       |\n");
    fmt::print("      | std::function      | não    | talvez| SIM       | fixa       |\n");
    fmt::print("      | function_ref (C++26)| não   | NÃO   | não-dono  | fixa       |\n");

    fmt::print("\n      Use `template <typename F>` quando:\n");
    fmt::print("        • a função CONSOME o chamável e retorna (algoritmos)\n");
    fmt::print("        • performance importa\n");
    fmt::print("        • é código de biblioteca genérico\n");

    fmt::print("\n      Use `std::function` quando:\n");
    fmt::print("        • precisa GUARDAR o chamável (callback, handler, fila)\n");
    fmt::print("        • precisa de heterogeneidade num container\n");
    fmt::print("        • a assinatura precisa aparecer numa fronteira de ABI\n");
    fmt::print("          (um template não pode atravessar um .so)\n");

    fmt::print("\n      ⚠  o erro mais comum: usar `std::function` como PARÂMETRO\n");
    fmt::print("         de uma função que só o chama e descarta.\n");
    fmt::print("           void ordenar(vector<int>&, std::function<bool(int,int)>);\n");
    fmt::print("         Isso força alocação e impede inlining sem nenhum ganho.\n");
    fmt::print("         O certo ali é `template <typename Comp>`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_o_problema();
    demo_como_funciona();
    demo_custo();
    demo_sbo();
    demo_quando();
}

} // namespace demo_function

// ─────────────────────────────────────────────────────────────────────────────
//  Contagem de alocações, usada por demo_sbo(). Definida em UMA TU só.
// ─────────────────────────────────────────────────────────────────────────────
void* operator new(std::size_t n) {
    if (demo_function::contador::ativo) ++demo_function::contador::alocacoes;
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc{};
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
