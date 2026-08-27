// =============================================================================
//  src/type_erasure/demo_alternativas.hpp
//
//  virtual vs CRTP vs variant vs type erasure — medido
//  ───────────────────────────────────────────────────
//  As quatro formas de "chamar a operação certa para o objeto certo" não são
//  intercambiáveis: cada uma abre mão de algo diferente. A tabela existe em
//  todo lugar; o que falta é o NÚMERO. Este arquivo mede.
//
//  ⚠ Sobre a medição: o compilador com -O2 devora benchmarks ingênuos. Cada
//    caso abaixo (a) trabalha sobre o MESMO vetor de dados, (b) acumula o
//    resultado num `volatile`, (c) roda várias repetições. Ainda assim, o
//    número certo é o que você medir NO SEU caso — ver src/chrono/demo_medicao.hpp.
//
//  Tópicos:
//  • Os quatro despachos, lado a lado
//  • Medição com o mesmo trabalho útil
//  • O problema da expressão: qual eixo você quer barato
//  • Quando cada um é a resposta
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <chrono>
#include <cstddef>
#include <memory>
#include <numeric>
#include <variant>
#include <vector>

namespace demo_alternativas {

constexpr std::size_t kN     = 200'000;
constexpr int         kRodadas = 20;

// ─────────────────────────────────────────────────────────────────────────────
//  (1) virtual — aberto para TIPOS, fechado para OPERAÇÕES
// ─────────────────────────────────────────────────────────────────────────────
namespace com_virtual {
struct Forma { virtual ~Forma() = default; virtual double area() const = 0; };
struct Circulo  : Forma { double r; explicit Circulo(double x) : r{x} {}
                          double area() const override { return 3.14159265 * r * r; } };
struct Quadrado : Forma { double l; explicit Quadrado(double x) : l{x} {}
                          double area() const override { return l * l; } };
}

// ─────────────────────────────────────────────────────────────────────────────
//  (2) CRTP — polimorfismo estático, ZERO indireção... e zero heterogeneidade
// ─────────────────────────────────────────────────────────────────────────────
namespace com_crtp {
template <typename D>
struct Forma { double area() const { return static_cast<const D&>(*this).area_impl(); } };
struct Circulo  : Forma<Circulo>  { double r; explicit Circulo(double x) : r{x} {}
                                    double area_impl() const { return 3.14159265 * r * r; } };
struct Quadrado : Forma<Quadrado> { double l; explicit Quadrado(double x) : l{x} {}
                                    double area_impl() const { return l * l; } };
}

// ─────────────────────────────────────────────────────────────────────────────
//  (3) variant — FECHADO para tipos, aberto para operações
// ─────────────────────────────────────────────────────────────────────────────
namespace com_variant {
struct Circulo  { double r; };
struct Quadrado { double l; };
using Forma = std::variant<Circulo, Quadrado>;

// O overload idiom — ver src/vocabulary_types/demo_variant.hpp
template <class... Ts> struct sobrecarga : Ts... { using Ts::operator()...; };
template <class... Ts> sobrecarga(Ts...) -> sobrecarga<Ts...>;

inline double area(const Forma& f) {
    return std::visit(sobrecarga{
        [](const Circulo&  c) { return 3.14159265 * c.r * c.r; },
        [](const Quadrado& q) { return q.l * q.l; },
    }, f);
}
}

// ─────────────────────────────────────────────────────────────────────────────
//  (4) type erasure — aberto para tipos, NÃO intrusivo
// ─────────────────────────────────────────────────────────────────────────────
namespace com_erasure {
struct Circulo  { double r; double area() const { return 3.14159265 * r * r; } };
struct Quadrado { double l; double area() const { return l * l; } };
// Nenhum dos dois herda de nada nem sabe que `Forma` existe.

class Forma {
    struct Conceito {
        virtual ~Conceito() = default;
        virtual double area() const = 0;
    };
    template <typename T> struct Modelo final : Conceito {
        T v;
        explicit Modelo(T x) : v{std::move(x)} {}
        double area() const override { return v.area(); }
    };
    std::unique_ptr<Conceito> p_;
public:
    template <typename T> Forma(T x)                        // NOLINT: implícito
        : p_{std::make_unique<Modelo<T>>(std::move(x))} {}
    double area() const { return p_->area(); }
};
}

// ─────────────────────────────────────────────────────────────────────────────
//  A medição
// ─────────────────────────────────────────────────────────────────────────────
void demo_medicao() {
    fmt::print("\n── 4.1 Os quatro despachos, medidos ──\n");

    // Mesmos dados para todos: alternando círculo e quadrado, para que a
    // predição de desvio não favoreça nenhum caso.
    std::vector<double> tamanhos(kN);
    for (std::size_t i = 0; i < kN; ++i) tamanhos[i] = 1.0 + (i % 7) * 0.1;

    volatile double sumidouro = 0.0;   // impede o compilador de descartar tudo

    auto cronometrar = [&](const char* rotulo, auto&& corpo) {
        corpo();                                       // aquecimento
        const auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < kRodadas; ++r) sumidouro = corpo();
        const auto t1 = std::chrono::steady_clock::now();
        const double ms =
            std::chrono::duration<double, std::milli>(t1 - t0).count() / kRodadas;
        fmt::print("        {:<34} {:7.3f} ms/rodada\n", rotulo, ms);
        return ms;
    };

    // ── (1) virtual ──
    std::vector<std::unique_ptr<com_virtual::Forma>> v1;
    v1.reserve(kN);
    for (std::size_t i = 0; i < kN; ++i)
        if (i % 2) v1.push_back(std::make_unique<com_virtual::Circulo>(tamanhos[i]));
        else       v1.push_back(std::make_unique<com_virtual::Quadrado>(tamanhos[i]));

    // ── (2) CRTP: dois vetores HOMOGÊNEOS — é o ponto ──
    std::vector<com_crtp::Circulo>  c2;
    std::vector<com_crtp::Quadrado> q2;
    for (std::size_t i = 0; i < kN; ++i)
        if (i % 2) c2.emplace_back(tamanhos[i]); else q2.emplace_back(tamanhos[i]);

    // ── (3) variant: por VALOR, contíguo, sem alocação ──
    std::vector<com_variant::Forma> v3;
    v3.reserve(kN);
    for (std::size_t i = 0; i < kN; ++i)
        if (i % 2) v3.emplace_back(com_variant::Circulo{tamanhos[i]});
        else       v3.emplace_back(com_variant::Quadrado{tamanhos[i]});

    // ── (4) type erasure ──
    std::vector<com_erasure::Forma> v4;
    v4.reserve(kN);
    for (std::size_t i = 0; i < kN; ++i)
        if (i % 2) v4.emplace_back(com_erasure::Circulo{tamanhos[i]});
        else       v4.emplace_back(com_erasure::Quadrado{tamanhos[i]});

    fmt::print("      somando {} áreas, média de {} rodadas:\n\n", kN, kRodadas);

    const double t_virt = cronometrar("virtual (unique_ptr<Base>)", [&] {
        double s = 0; for (const auto& f : v1) s += f->area(); return s;
    });
    const double t_crtp = cronometrar("CRTP (vetores homogêneos)", [&] {
        double s = 0;
        for (const auto& c : c2) s += c.area();
        for (const auto& q : q2) s += q.area();
        return s;
    });
    const double t_var = cronometrar("variant + visit", [&] {
        double s = 0; for (const auto& f : v3) s += com_variant::area(f); return s;
    });
    const double t_te = cronometrar("type erasure (unique_ptr interno)", [&] {
        double s = 0; for (const auto& f : v4) s += f.area(); return s;
    });

    const double base = t_crtp;
    fmt::print("\n      relativo ao CRTP (o mais rápido por construção):\n");
    fmt::print("        virtual       {:5.1f}×\n", t_virt / base);
    fmt::print("        CRTP          {:5.1f}×\n", 1.0);
    fmt::print("        variant       {:5.1f}×\n", t_var  / base);
    fmt::print("        type erasure  {:5.1f}×\n", t_te   / base);

    fmt::print("\n      Memória por elemento:\n");
    fmt::print("        virtual      : {:2} B (ponteiro) + heap: vptr + dado\n",
               sizeof(std::unique_ptr<com_virtual::Forma>));
    fmt::print("        CRTP         : {:2} B, contíguo, SEM vptr\n",
               sizeof(com_crtp::Circulo));
    fmt::print("        variant      : {:2} B, contíguo (maior alternativa + tag)\n",
               sizeof(com_variant::Forma));
    fmt::print("        type erasure : {:2} B (ponteiro) + heap\n",
               sizeof(com_erasure::Forma));

    fmt::print("\n      A diferença que domina o resultado NÃO é o despacho — é a\n");
    fmt::print("      LOCALIDADE. `variant` e CRTP percorrem memória contígua;\n");
    fmt::print("      `virtual` e type erasure percorrem um vetor de ponteiros que\n");
    fmt::print("      apontam para lugares espalhados no heap. Um cache miss custa\n");
    fmt::print("      ~200 ciclos; uma chamada indireta bem predita, ~2.\n");
    fmt::print("      Ver src/performance/demo_cache.hpp para essa medição isolada.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  O problema da expressão
// ─────────────────────────────────────────────────────────────────────────────
void demo_expressao() {
    fmt::print("\n── 4.2 O problema da expressão ──\n");

    fmt::print("      Você tem TIPOS (círculo, quadrado…) e OPERAÇÕES (área,\n");
    fmt::print("      perímetro, desenhar…). Só dá para deixar UM dos dois eixos\n");
    fmt::print("      barato de estender:\n\n");

    fmt::print("      | abordagem     | tipo novo         | operação nova       |\n");
    fmt::print("      |---------------|-------------------|---------------------|\n");
    fmt::print("      | virtual       | fácil: 1 classe   | CARO: mexe na base  |\n");
    fmt::print("      |               |                   | e em TODO derivado  |\n");
    fmt::print("      | variant+visit | CARO: recompila   | fácil: outro visitor|\n");
    fmt::print("      |               | todos os visitors |                     |\n");
    fmt::print("      | type erasure  | fácil, e SEM      | CARO: mexe no       |\n");
    fmt::print("      |               | tocar no tipo     | Conceito            |\n");
    fmt::print("      | CRTP          | fácil             | fácil               |\n");
    fmt::print("      |               | (mas sem heterogeneidade em runtime)    |\n\n");

    fmt::print("      O CRTP parece ganhar dos dois lados porque não resolve o\n");
    fmt::print("      mesmo problema: ele NÃO dá um container heterogêneo. Se você\n");
    fmt::print("      precisa de `vector<Forma>` com tipos diferentes decididos em\n");
    fmt::print("      runtime, o CRTP simplesmente não é candidato.\n\n");

    fmt::print("      Como decidir, na ordem:\n");
    fmt::print("        1. os tipos são conhecidos e a lista é curta e estável?\n");
    fmt::print("           → `variant`. Contíguo, sem alocação, exaustividade\n");
    fmt::print("             checada em compilação.\n");
    fmt::print("        2. os tipos são abertos e você controla as classes?\n");
    fmt::print("           → herança + virtual. É o mais simples de ler.\n");
    fmt::print("        3. os tipos são abertos e você NÃO controla as classes\n");
    fmt::print("           (`int`, `std::string`, tipos de terceiros)?\n");
    fmt::print("           → type erasure.\n");
    fmt::print("        4. não há heterogeneidade em runtime, só reuso de código?\n");
    fmt::print("           → template/CRTP, e nem pense em vtable.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_medicao();
    demo_expressao();
}

}  // namespace demo_alternativas
