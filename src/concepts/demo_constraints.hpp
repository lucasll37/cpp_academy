// =============================================================================
//  src/concepts/demo_constraints.hpp
//
//  Constraints Avançadas
//  ──────────────────────
//  Tópicos:
//  • Subsumption — como o compilador escolhe a sobrecarga mais especializada
//  • Concepts em class templates e métodos condicionais
//  • Concepts em lambdas (C++20)
//  • Concepts para restringir parâmetros não-tipo (NTTP)
//  • Partial ordering de templates por constraints
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <concepts>
#include <string>
#include <vector>
#include <type_traits>

namespace demo_constraints {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Subsumption — o compilador prefere a constraint mais restrita
//
//  Quando múltiplas sobrecargas são viáveis, o compilador escolhe a que
//  tem constraints "mais fortes" (que implicam as outras).
//
//  A implica B quando todo tipo que satisfaz A também satisfaz B.
//  Para o compilador detectar isso, A deve ser definido EM TERMOS de B.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
concept Numerico = std::integral<T> || std::floating_point<T>;

// std::integral é mais restrito que Numerico: todo integral é Numerico,
// mas nem todo Numerico é integral. Logo std::integral subsume Numerico.

template <Numerico T>
std::string categoria(T) { return "numerico generico"; }

template <std::integral T>
std::string categoria(T) { return "inteiro (mais especializado)"; }

template <std::floating_point T>
std::string categoria(T) { return "ponto-flutuante (mais especializado)"; }

void demo_subsumption() {
    fmt::print("\n── 3.1 Subsumption — overload mais especializado ──\n");

    fmt::print("  categoria(42)    = {}\n", categoria(42));
    fmt::print("  categoria(3.14)  = {}\n", categoria(3.14));
    fmt::print("  categoria(2.0f)  = {}\n", categoria(2.0f));

    fmt::print("\n  Regra: se A subsume B, o compilador prefere a sobrecarga com A.\n");
    fmt::print("  std::integral subsume Numerico porque Numerico inclui integral.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Concepts em class templates
//     Métodos de uma classe podem ser condicionalmente disponíveis
//     usando requires em cada método.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
class Caixa {
public:
    explicit Caixa(T val) : valor_(val) {}

    // Método sempre disponível
    const T& valor() const { return valor_; }

    // Método disponível apenas para tipos numéricos
    T dobrar() const requires Numerico<T> {
        return valor_ * 2;
    }

    // Método disponível apenas para tipos com operador <
    bool menor_que(const Caixa& outro) const
        requires std::totally_ordered<T>
    {
        return valor_ < outro.valor_;
    }

    // Método disponível apenas para strings
    std::size_t tamanho() const
        requires std::same_as<T, std::string>
    {
        return valor_.size();
    }

private:
    T valor_;
};

void demo_class_template() {
    fmt::print("\n── 3.2 Concepts em class templates ──\n");

    Caixa<int> ci(21);
    fmt::print("  Caixa<int>(21).dobrar()      = {}\n", ci.dobrar());
    fmt::print("  Caixa<int>(21) < Caixa<int>(30): {}\n",
               ci.menor_que(Caixa<int>(30)));

    Caixa<double> cd(3.14);
    fmt::print("  Caixa<double>(3.14).dobrar() = {:.2f}\n", cd.dobrar());

    Caixa<std::string> cs(std::string("academy"));
    fmt::print("  Caixa<string>.tamanho()      = {}\n", cs.tamanho());
    // cs.dobrar();  // erro: 'string' não satisfaz 'Numerico'
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Concepts em lambdas (C++20)
//     Lambdas podem usar `auto` restrito por concept como parâmetro.
// ─────────────────────────────────────────────────────────────────────────────

void demo_lambdas() {
    fmt::print("\n── 3.3 Concepts em lambdas ──\n");

    // Lambda que aceita apenas tipos integrais
    auto quadrado_inteiro = [](std::integral auto x) {
        return x * x;
    };

    // Lambda que aceita apenas tipos floating_point
    auto raiz_aproximada = [](std::floating_point auto x) {
        // Newton-Raphson simplificado
        auto r = x / 2.0;
        for (int i = 0; i < 10; ++i) r = (r + x / r) / 2.0;
        return r;
    };

    // Lambda com concept composto inline
    auto somar_numericos = [](Numerico auto a, Numerico auto b) {
        return a + b;
    };

    fmt::print("  quadrado_inteiro(7)      = {}\n", quadrado_inteiro(7));
    fmt::print("  quadrado_inteiro(10L)    = {}\n", quadrado_inteiro(10L));
    fmt::print("  raiz_aproximada(2.0)     = {:.6f}\n", raiz_aproximada(2.0));
    fmt::print("  raiz_aproximada(9.0f)    = {:.4f}\n", raiz_aproximada(9.0f));
    fmt::print("  somar_numericos(3, 4)    = {}\n", somar_numericos(3, 4));
    fmt::print("  somar_numericos(1.5,2.5) = {:.1f}\n", somar_numericos(1.5, 2.5));
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Concepts para parâmetros não-tipo (NTTP — Non-Type Template Parameters)
//     C++20 permite NTTP de qualquer tipo literal, não apenas integrais.
// ─────────────────────────────────────────────────────────────────────────────

// Concept que valida um NTTP inteiro positivo
template <auto N>
concept Positivo = (N > 0);

// Array de tamanho fixo com tamanho validado em compile-time
template <typename T, std::size_t N>
    requires (N > 0)
class ArrayFixo {
public:
    ArrayFixo() { dados_.fill(T{}); }
    explicit ArrayFixo(T val) { dados_.fill(val); }

    T& operator[](std::size_t i) { return dados_[i]; }
    const T& operator[](std::size_t i) const { return dados_[i]; }
    constexpr std::size_t tamanho() const { return N; }

    void imprimir() const {
        fmt::print("  ArrayFixo<{}>: [", N);
        for (std::size_t i = 0; i < N; ++i) {
            fmt::print("{}{}", dados_[i], i + 1 < N ? ", " : "");
        }
        fmt::print("]\n");
    }

private:
    std::array<T, N> dados_;
};

void demo_nttp() {
    fmt::print("\n── 3.4 Concepts em NTTP ──\n");

    ArrayFixo<int, 5> arr(0);
    for (std::size_t i = 0; i < arr.tamanho(); ++i)
        arr[i] = static_cast<int>(i * i);
    arr.imprimir();

    ArrayFixo<double, 3> flt(1.0);
    flt[0] = 1.1; flt[1] = 2.2; flt[2] = 3.3;
    flt.imprimir();

    // ArrayFixo<int, 0> invalido;  // erro: constraint (N > 0) não satisfeita
    fmt::print("  (ArrayFixo<T, 0> não compilaria — constraint N > 0)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Concepts para design de APIs type-safe
//     Um padrão comum: usar concepts para documentar e enforçar contratos.
// ─────────────────────────────────────────────────────────────────────────────

// Concept: tipo que se comporta como um "handler" de eventos
template <typename H, typename Event>
concept EventHandler = requires(H handler, const Event& event) {
    { handler.on_event(event) } -> std::same_as<void>;
    { handler.nome() }          -> std::convertible_to<std::string>;
};

struct EventoClick {
    int x, y;
};

struct EventoTeclado {
    char tecla;
};

struct HandlerUI {
    void on_event(const EventoClick& e) {
        fmt::print("    HandlerUI: click em ({}, {})\n", e.x, e.y);
    }
    void on_event(const EventoTeclado& e) {
        fmt::print("    HandlerUI: tecla '{}'\n", e.tecla);
    }
    std::string nome() const { return "HandlerUI"; }
};

template <typename H, typename E>
    requires EventHandler<H, E>
void disparar_evento(H& handler, const E& event) {
    fmt::print("  [{}] processando evento...\n", handler.nome());
    handler.on_event(event);
}

void demo_api_design() {
    fmt::print("\n── 3.5 Concepts para design de APIs type-safe ──\n");

    HandlerUI ui;
    disparar_evento(ui, EventoClick{100, 200});
    disparar_evento(ui, EventoTeclado{'A'});

    fmt::print("  EventHandler<HandlerUI, EventoClick>:    {}\n",
               EventHandler<HandlerUI, EventoClick>);
    fmt::print("  EventHandler<HandlerUI, EventoTeclado>:  {}\n",
               EventHandler<HandlerUI, EventoTeclado>);
    fmt::print("  EventHandler<int, EventoClick>:          {}\n",
               EventHandler<int, EventoClick>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_subsumption();
    demo_class_template();
    demo_lambdas();
    demo_nttp();
    demo_api_design();
}

} // namespace demo_constraints
