// =============================================================================
//  src/operators/demo_literais.hpp
//
//  Literais definidos pelo usuário
//  ───────────────────────────────
//  Um literal de usuário (UDL) permite escrever `5_km` e obter um objeto do
//  seu tipo — não um `int` que você espera que seja quilômetros.
//
//  O ganho não é sintático: é de CORREÇÃO. Com um tipo forte por unidade, o
//  compilador impede somar metros com segundos. É a mesma ideia das dimensões
//  no tipo (src/compile_time/demo_nttp.hpp), aplicada a grandezas físicas.
//
//  Tópicos:
//  • A sintaxe e a regra do underscore
//  • As quatro formas de operator""
//  • Um sistema de unidades com dimensões verificadas
//  • Os literais da biblioteca padrão (chrono, string, complex)
//  • Literais de template (C++20) — validação em compilação
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <chrono>
#include <complex>
#include <cstddef>
#include <ratio>
#include <string>
#include <string_view>
#include <type_traits>

namespace demo_literais {

// ─────────────────────────────────────────────────────────────────────────────
//  1. A sintaxe e as quatro formas
//
//     A regra do underscore: literais que NÃO começam com `_` são reservados
//     para a biblioteca padrão. Escrever `operator""km` é tecnicamente
//     permitido mas é UB reservado — sempre use `_km`.
// ─────────────────────────────────────────────────────────────────────────────
namespace formas {

// (a) Literal INTEIRO cru: recebe unsigned long long
constexpr long long operator""_dobro(unsigned long long v) {
    return static_cast<long long>(v) * 2;
}

// (b) Literal de PONTO FLUTUANTE: recebe long double
constexpr double operator""_porcento(long double v) {
    return static_cast<double>(v) / 100.0;
}

// (c) Literal de STRING: recebe (const char*, size_t)
constexpr std::size_t operator""_tamanho(const char*, std::size_t n) {
    return n;
}

// (d) Literal de CARACTERE: recebe char
constexpr int operator""_codigo(char c) {
    return static_cast<int>(c);
}

} // namespace formas

void demo_formas() {
    fmt::print("\n── 5.1 As quatro formas ──\n");

    using namespace formas;

    fmt::print("      21_dobro          = {}   (unsigned long long)\n", 21_dobro);
    fmt::print("      15.5_porcento     = {}  (long double)\n",         15.5_porcento);
    fmt::print("      \"abcde\"_tamanho   = {}   (const char*, size_t)\n", "abcde"_tamanho);
    fmt::print("      'A'_codigo        = {}  (char)\n",                'A'_codigo);

    // Todos são constexpr → utilizáveis em contexto de compilação
    static_assert(21_dobro == 42);
    static_assert("abcde"_tamanho == 5);
    fmt::print("      static_assert(21_dobro == 42) ✓  — os UDLs são constexpr\n");

    fmt::print("\n      | sufixo em… | assinatura do operator\"\"           |\n");
    fmt::print("      |------------|-----------------------------------|\n");
    fmt::print("      | 42_x       | (unsigned long long)              |\n");
    fmt::print("      | 4.2_x      | (long double)                     |\n");
    fmt::print("      | \"abc\"_x    | (const char*, std::size_t)        |\n");
    fmt::print("      | 'a'_x      | (char)                            |\n");
    fmt::print("      | 42_x cru   | (const char*)  ← recebe os DÍGITOS |\n");

    fmt::print("\n      ⚠  A REGRA DO UNDERSCORE: sufixos sem `_` inicial são\n");
    fmt::print("         RESERVADOS para a biblioteca padrão. `operator\"\"km`\n");
    fmt::print("         compila hoje e pode colidir amanhã. Use sempre `_km`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Um sistema de unidades com dimensões verificadas
//
//     Cada grandeza vira um tipo. O compilador então impede as operações que
//     não fazem sentido dimensional — e DERIVA o tipo das que fazem.
// ─────────────────────────────────────────────────────────────────────────────
namespace unidades {

// As dimensões são expoentes no TIPO: Grandeza<1,0,0> é comprimento,
// Grandeza<1,0,-1> é velocidade (comprimento / tempo), etc.
template <int Comprimento, int Massa, int Tempo>
class Grandeza {
public:
    constexpr explicit Grandeza(double v) : valor_(v) {}
    constexpr double valor() const { return valor_; }

    // Soma e subtração exigem dimensões IDÊNTICAS — garantido pelo tipo
    constexpr Grandeza operator+(Grandeza o) const { return Grandeza{valor_ + o.valor_}; }
    constexpr Grandeza operator-(Grandeza o) const { return Grandeza{valor_ - o.valor_}; }

    // Multiplicação SOMA os expoentes; divisão SUBTRAI. O tipo do resultado
    // é calculado pelo compilador.
    template <int C2, int M2, int T2>
    constexpr auto operator*(Grandeza<C2, M2, T2> o) const {
        return Grandeza<Comprimento + C2, Massa + M2, Tempo + T2>{valor_ * o.valor()};
    }
    template <int C2, int M2, int T2>
    constexpr auto operator/(Grandeza<C2, M2, T2> o) const {
        return Grandeza<Comprimento - C2, Massa - M2, Tempo - T2>{valor_ / o.valor()};
    }

    constexpr auto operator<=>(const Grandeza&) const = default;

private:
    double valor_;
};

using Comprimento = Grandeza<1, 0,  0>;
using Massa       = Grandeza<0, 1,  0>;
using Tempo       = Grandeza<0, 0,  1>;
using Velocidade  = Grandeza<1, 0, -1>;
using Aceleracao  = Grandeza<1, 0, -2>;
using Forca       = Grandeza<1, 1, -2>;      // massa × aceleração

// Os literais que dão a sintaxe agradável
constexpr Comprimento operator""_m  (long double v) { return Comprimento{static_cast<double>(v)}; }
constexpr Comprimento operator""_km (long double v) { return Comprimento{static_cast<double>(v) * 1000}; }
constexpr Comprimento operator""_m  (unsigned long long v) { return Comprimento{static_cast<double>(v)}; }
constexpr Comprimento operator""_km (unsigned long long v) { return Comprimento{static_cast<double>(v) * 1000}; }
constexpr Massa       operator""_kg (unsigned long long v) { return Massa{static_cast<double>(v)}; }
constexpr Tempo       operator""_s  (unsigned long long v) { return Tempo{static_cast<double>(v)}; }
constexpr Tempo       operator""_h  (unsigned long long v) { return Tempo{static_cast<double>(v) * 3600}; }

} // namespace unidades

void demo_unidades() {
    fmt::print("\n── 5.2 Unidades com dimensões verificadas ──\n");

    using namespace unidades;

    constexpr auto distancia = 100_km;
    constexpr auto tempo     = 2_h;
    constexpr auto velocidade = distancia / tempo;      // tipo DERIVADO

    static_assert(std::is_same_v<decltype(velocidade), const Velocidade>);
    fmt::print("      100_km / 2_h = {:.2f} m/s  (tipo Velocidade, derivado)\n",
               velocidade.valor());

    constexpr auto massa      = 70_kg;
    constexpr auto aceleracao = Aceleracao{9.81};
    constexpr auto peso       = massa * aceleracao;

    static_assert(std::is_same_v<decltype(peso), const Forca>);
    fmt::print("      70_kg * 9.81 m/s² = {:.2f} N  (tipo Forca, derivado)\n",
               peso.valor());

    // A soma de dimensões iguais funciona
    constexpr auto total = 5_km + 300_m;
    fmt::print("      5_km + 300_m = {:.0f} m\n", total.valor());

    // E a que não faz sentido não compila:
    // auto absurdo = 5_km + 2_s;         // ✗ Grandeza<1,0,0> + Grandeza<0,0,1>
    fmt::print("      5_km + 2_s   → NÃO COMPILA (dimensões incompatíveis)\n");

    // Comparação também é dimensionalmente segura
    fmt::print("      5_km > 300_m → {}\n", 5_km > 300_m);
    // 5_km > 2_s;                        // ✗ não compila

    fmt::print("\n      Todo o cálculo acima aconteceu na COMPILAÇÃO — os\n");
    fmt::print("      `static_assert` provam os tipos, e o `constexpr` os valores.\n");
    fmt::print("      Em runtime, `Grandeza` é apenas um `double`:\n");
    fmt::print("        sizeof(Comprimento) = {}, sizeof(double) = {}\n",
               sizeof(Comprimento), sizeof(double));
    fmt::print("      Segurança dimensional a CUSTO ZERO.\n");
    fmt::print("      (é assim que funcionam mp-units, Boost.Units e o\n");
    fmt::print("       `std::chrono`, que é um sistema de unidades de tempo)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Os literais da biblioteca padrão
//
//     Todos vivem em namespaces `std::literals::*`, e o `using` é seletivo de
//     propósito: você importa só os que precisa.
// ─────────────────────────────────────────────────────────────────────────────
void demo_padrao() {
    fmt::print("\n── 5.3 Os literais da biblioteca padrão ──\n");

    {   // <chrono> — o exemplo mais usado
        using namespace std::chrono_literals;

        constexpr auto meia_hora  = 30min;
        constexpr auto um_segundo = 1s;
        constexpr auto rapido     = 250ms;

        fmt::print("      chrono: 30min = {} s\n",
                   std::chrono::duration_cast<std::chrono::seconds>(meia_hora).count());
        fmt::print("              1s + 250ms = {} ms\n",
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       um_segundo + rapido).count());
        // 30min + 1s compila e converte sozinho — é o mesmo sistema de unidades
        fmt::print("      ↑ `chrono` É um sistema de unidades: a conversão entre\n");
        fmt::print("        escalas é automática e verificada pelo tipo.\n");
    }

    {   // <string> — o que resolve a ambiguidade de const char*
        using namespace std::string_literals;
        using namespace std::string_view_literals;

        auto s  = "texto"s;         // std::string, não const char*
        auto sv = "vista"sv;        // std::string_view

        fmt::print("      string:  \"texto\"s  → std::string de {} chars\n", s.size());
        fmt::print("      s_view:  \"vista\"sv → std::string_view de {} chars\n", sv.size());

        // O caso em que isso importa: strings com '\0' no meio
        const auto com_nulo = "a\0b"s;
        fmt::print("      \"a\\0b\"s tem {} caracteres (com o \\0 no meio!)\n",
                   com_nulo.size());
        fmt::print("      já `strlen(\"a\\0b\")` daria 1 — o literal `s` preserva.\n");
    }

    {   // <complex>
        using namespace std::complex_literals;
        const auto z = 3.0 + 4.0i;
        fmt::print("      complex: 3.0 + 4.0i → módulo {}\n", std::abs(z));
    }

    fmt::print("\n      | header    | namespace                      | sufixos       |\n");
    fmt::print("      |-----------|--------------------------------|---------------|\n");
    fmt::print("      | <chrono>  | std::chrono_literals           | h min s ms us |\n");
    fmt::print("      | <string>  | std::string_literals           | s             |\n");
    fmt::print("      | <string_view>| std::string_view_literals   | sv            |\n");
    fmt::print("      | <complex> | std::complex_literals          | i if il       |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Literais de template — validação em compilação
//
//     A forma `template <char... C> operator""_x()` recebe os DÍGITOS como
//     parâmetros de template. Isso permite validar o literal na compilação —
//     rejeitando valores fora de faixa antes de o programa existir.
// ─────────────────────────────────────────────────────────────────────────────
namespace validado {

// Converte a sequência de dígitos num valor, em compilação
template <char... Digitos>
constexpr int para_inteiro() {
    int r = 0;
    // Fold expression sobre os caracteres do literal
    ((r = r * 10 + (Digitos - '0')), ...);
    return r;
}

// Um literal para porcentagem que RECUSA valores fora de 0..100
template <char... Digitos>
constexpr double operator""_pct() {
    constexpr int v = para_inteiro<Digitos...>();
    static_assert(v >= 0 && v <= 100,
                  "porcentagem precisa estar entre 0 e 100");
    return v / 100.0;
}

} // namespace validado

void demo_template_literal() {
    fmt::print("\n── 5.4 Literais de template (validação) ──\n");

    using namespace validado;

    constexpr double meio  = 50_pct;
    constexpr double tudo  = 100_pct;
    constexpr double nada  = 0_pct;

    fmt::print("      50_pct  = {}\n",  meio);
    fmt::print("      100_pct = {}\n",  tudo);
    fmt::print("      0_pct   = {}\n",  nada);

    // constexpr double erro = 150_pct;
    // → error: static assertion failed: porcentagem precisa estar entre 0 e 100
    fmt::print("      150_pct → NÃO COMPILA: \"porcentagem precisa estar entre\n");
    fmt::print("                0 e 100\"\n");

    fmt::print("\n      A assinatura:\n");
    fmt::print("        template <char... Digitos>\n");
    fmt::print("        constexpr double operator\"\"_pct();\n");
    fmt::print("      Os dígitos chegam como parâmetros de TEMPLATE, então\n");
    fmt::print("      podem ser validados por `static_assert`.\n");
    fmt::print("      É a mesma ideia do `consteval` validando entrada em\n");
    fmt::print("      src/compile_time/demo_consteval.hpp — aqui aplicada à\n");
    fmt::print("      sintaxe de literal.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_formas();
    demo_unidades();
    demo_padrao();
    demo_template_literal();
}

} // namespace demo_literais
