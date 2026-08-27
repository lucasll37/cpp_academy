// =============================================================================
//  src/compile_time/demo_nttp.hpp
//
//  NTTP — parâmetros de template que são VALORES
//  ─────────────────────────────────────────────
//  Um template pode receber tipos (`template <typename T>`) e também VALORES
//  (`template <int N>`). O segundo caso são os Non-Type Template Parameters.
//
//  O ponto importante: o valor faz parte do TIPO. `Matriz<3,3>` e `Matriz<4,4>`
//  são tipos diferentes, e o compilador consegue verificar em compilação que
//  você não está multiplicando dimensões incompatíveis.
//
//  O C++20 ampliou muito o que pode ser NTTP: antes só inteiros, enums,
//  ponteiros e referências; agora também tipos de classe literais — o que
//  finalmente permitiu strings como parâmetro de template.
//
//  Tópicos:
//  • NTTP clássicos e o que muda em relação a um parâmetro comum
//  • Dimensões no tipo: erros de álgebra viram erros de compilação
//  • NTTP de tipo classe (C++20) e o fixed_string
//  • `auto` como NTTP
//  • Onde o custo aparece: uma instanciação por valor
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace demo_nttp {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O básico — o valor entra no tipo
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, std::size_t N>
class BufferFixo {
public:
    constexpr std::size_t capacidade() const { return N; }   // constante, não campo

    constexpr void adicionar(T v) {
        if (tamanho_ < N) dados_[tamanho_++] = v;
    }
    constexpr T operator[](std::size_t i) const { return dados_[i]; }
    constexpr std::size_t tamanho() const { return tamanho_; }

private:
    std::array<T, N> dados_{};
    std::size_t      tamanho_ = 0;
};

void demo_basico() {
    fmt::print("\n── 4.1 O valor entra no tipo ──\n");

    BufferFixo<int, 4>  pequeno;
    BufferFixo<int, 16> grande;

    fmt::print("      sizeof(BufferFixo<int,4>)  = {}\n",  sizeof(pequeno));
    fmt::print("      sizeof(BufferFixo<int,16>) = {}\n",  sizeof(grande));
    fmt::print("      → são TIPOS DIFERENTES: o 4 e o 16 estão no tipo, não\n");
    fmt::print("        num campo. `capacidade()` não lê memória nenhuma.\n");

    // pequeno = grande;              // ✗ não compila: tipos diferentes

    // E como tudo é constexpr, o buffer inteiro pode ser preenchido na compilação
    constexpr auto preenchido = [] {
        BufferFixo<int, 5> b;
        for (int i = 1; i <= 5; ++i) b.adicionar(i * i);
        return b;
    }();
    static_assert(preenchido.tamanho() == 5);
    static_assert(preenchido[4] == 25);
    fmt::print("      buffer preenchido na compilação: ");
    for (std::size_t i = 0; i < preenchido.tamanho(); ++i) fmt::print("{} ", preenchido[i]);
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Dimensões no tipo — erros de álgebra em compilação
//
//     Este é o argumento mais forte a favor de NTTP: transformar uma classe de
//     erro de runtime numa de compilação.
// ─────────────────────────────────────────────────────────────────────────────
template <std::size_t Linhas, std::size_t Colunas>
class Matriz {
public:
    constexpr Matriz() = default;

    constexpr double& operator()(std::size_t i, std::size_t j) {
        return dados_[i * Colunas + j];
    }
    constexpr double operator()(std::size_t i, std::size_t j) const {
        return dados_[i * Colunas + j];
    }

    // A multiplicação só compila se as dimensões internas casarem.
    // O resultado tem o tipo certo, derivado das entradas.
    template <std::size_t OutrasColunas>
    constexpr Matriz<Linhas, OutrasColunas>
    operator*(const Matriz<Colunas, OutrasColunas>& o) const {
        Matriz<Linhas, OutrasColunas> r;
        for (std::size_t i = 0; i < Linhas; ++i)
            for (std::size_t j = 0; j < OutrasColunas; ++j)
                for (std::size_t k = 0; k < Colunas; ++k)
                    r(i, j) += (*this)(i, k) * o(k, j);
        return r;
    }

    static constexpr std::size_t linhas  = Linhas;
    static constexpr std::size_t colunas = Colunas;

private:
    std::array<double, Linhas * Colunas> dados_{};
};

void demo_dimensoes() {
    fmt::print("\n── 4.2 Dimensões no tipo ──\n");

    Matriz<2, 3> a;
    Matriz<3, 4> b;
    Matriz<2, 2> c;

    a(0, 0) = 1; a(0, 1) = 2; a(0, 2) = 3;
    a(1, 0) = 4; a(1, 1) = 5; a(1, 2) = 6;
    for (std::size_t i = 0; i < 3; ++i) b(i, i) = 1;      // parcialmente identidade

    auto prod = a * b;               // Matriz<2,3> * Matriz<3,4> → Matriz<2,4>

    fmt::print("      Matriz<2,3> * Matriz<3,4> → Matriz<{},{}>\n",
               decltype(prod)::linhas, decltype(prod)::colunas);
    fmt::print("      resultado[0][0..2] = {} {} {}\n",
               prod(0, 0), prod(0, 1), prod(0, 2));

    // auto erro = a * c;            // ✗ Matriz<2,3> * Matriz<2,2>
    fmt::print("      `a * c` (Matriz<2,3> * Matriz<2,2>) → NÃO COMPILA\n");
    fmt::print("      A dimensão interna não casa, e o compilador SABE disso.\n");

    fmt::print("\n      Sem NTTP, as dimensões seriam campos e o erro só\n");
    fmt::print("      apareceria em runtime — provavelmente como assert, ou\n");
    fmt::print("      pior, como leitura fora dos limites.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. NTTP de tipo classe (C++20) — strings como parâmetro de template
//
//     A regra: o tipo precisa ser um "structural type" — todos os membros
//     públicos, sem invariantes escondidos, comparável estruturalmente.
//     Um array de char embrulhado numa struct atende.
// ─────────────────────────────────────────────────────────────────────────────
template <std::size_t N>
struct StringFixa {
    char dados[N]{};

    // O construtor consteval a partir de literal é o que permite escrever
    // `Rotulado<"texto">` — o literal vira uma StringFixa<N>.
    consteval StringFixa(const char (&lit)[N]) {
        std::copy_n(lit, N, dados);
    }

    constexpr std::string_view view() const { return {dados, N - 1}; }  // sem o '\0'
    constexpr std::size_t tamanho() const { return N - 1; }

    // Comparação estrutural: obrigatória para ser NTTP
    constexpr bool operator==(const StringFixa&) const = default;
};

// Deduction guide para o tamanho vir do literal
template <std::size_t N> StringFixa(const char (&)[N]) -> StringFixa<N>;

// Agora dá para parametrizar por TEXTO. O nome vira parte do tipo.
template <StringFixa Nome, typename T>
class Campo {
public:
    constexpr explicit Campo(T v) : valor_(v) {}

    void imprimir() const {
        fmt::print("      {:<10} = {}\n", Nome.view(), valor_);
    }

    static constexpr std::string_view nome() { return Nome.view(); }

private:
    T valor_;
};

void demo_string_nttp() {
    fmt::print("\n── 4.3 NTTP de tipo classe (C++20) ──\n");

    Campo<"largura", int>    largura{1920};
    Campo<"altura",  int>    altura{1080};
    Campo<"escala",  double> escala{1.5};

    largura.imprimir();
    altura.imprimir();
    escala.imprimir();

    // O nome é constante de compilação: dá para usar em static_assert
    static_assert(decltype(largura)::nome() == "largura");
    static_assert(decltype(largura)::nome().size() == 7);
    fmt::print("      static_assert(Campo<\"largura\",int>::nome() == \"largura\") ✓\n");

    // E `Campo<"largura",int>` e `Campo<"altura",int>` são tipos DIFERENTES
    fmt::print("      Campo<\"largura\",int> e Campo<\"altura\",int> são tipos\n");
    fmt::print("      distintos — trocá-los é erro de compilação, não de lógica.\n");

    fmt::print("\n      A regra para ser NTTP de tipo classe (\"structural type\"):\n");
    fmt::print("        • todos os membros de dados públicos\n");
    fmt::print("        • sem membros private/protected, sem virtual\n");
    fmt::print("        • os membros também precisam ser structural\n");
    fmt::print("      Por isso `std::string` NÃO pode ser NTTP: ele tem membros\n");
    fmt::print("      privados e aloca. Daí a necessidade do StringFixa.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. `auto` como NTTP — quando o tipo do valor não importa
// ─────────────────────────────────────────────────────────────────────────────
template <auto Valor>
struct Constante {
    static constexpr auto valor = Valor;
    using tipo = decltype(Valor);
};

// Uso prático: uma tag que carrega qualquer constante
template <auto Limite>
constexpr bool dentro_do_limite(decltype(Limite) x) { return x <= Limite; }

enum class Nivel { Baixo, Medio, Alto };

void demo_auto_nttp() {
    fmt::print("\n── 4.4 `auto` como NTTP ──\n");

    fmt::print("      Constante<42>::valor       = {}  (tipo int)\n", Constante<42>::valor);
    fmt::print("      Constante<3.5>::valor      = {}  (tipo double)\n", Constante<3.5>::valor);
    fmt::print("      Constante<'x'>::valor      = {}  (tipo char)\n", Constante<'x'>::valor);
    fmt::print("      Constante<Nivel::Alto>::valor = {} (tipo enum)\n",
               static_cast<int>(Constante<Nivel::Alto>::valor));

    static_assert(std::is_same_v<Constante<42>::tipo,  int>);
    static_assert(std::is_same_v<Constante<3.5>::tipo, double>);

    static_assert(dentro_do_limite<100>(50));
    static_assert(!dentro_do_limite<100>(150));
    fmt::print("      dentro_do_limite<100>(50)  = {}\n",  dentro_do_limite<100>(50));
    fmt::print("      dentro_do_limite<100>(150) = {}\n",  dentro_do_limite<100>(150));

    fmt::print("      `auto` evita escrever um template para cada tipo de valor.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O custo: uma instanciação por VALOR
//
//     Cada combinação distinta de argumentos de template gera código novo.
//     Com NTTP, "combinação distinta" inclui cada VALOR — o que pode explodir.
// ─────────────────────────────────────────────────────────────────────────────
void demo_custo() {
    fmt::print("\n── 4.5 O custo ──\n");

    fmt::print("      Cada valor distinto gera uma instanciação NOVA:\n");
    fmt::print("        BufferFixo<int,4>, <int,8>, <int,16> → 3 classes completas\n");
    fmt::print("        Matriz<2,3>, <3,4>, <2,4>            → 3 classes completas\n");

    fmt::print("\n      Consequências práticas:\n");
    fmt::print("        • binário maior (code bloat)\n");
    fmt::print("        • compilação mais lenta\n");
    fmt::print("        • cache de instruções sob pressão num loop que alterna\n");
    fmt::print("          entre instanciações diferentes\n");

    fmt::print("\n      Quando NÃO usar NTTP:\n");
    fmt::print("        • o valor vem de configuração, arquivo ou entrada\n");
    fmt::print("        • há muitos valores possíveis (`Buffer<n>` para n arbitrário)\n");
    fmt::print("        • você só quer evitar uma alocação — aí use um `span`\n");
    fmt::print("          ou uma capacidade em runtime\n");

    fmt::print("\n      Quando SIM:\n");
    fmt::print("        • poucos valores, conhecidos e fixos (dimensões de matriz\n");
    fmt::print("          de gráficos 3D: 2, 3, 4)\n");
    fmt::print("        • o valor participa da CORREÇÃO (dimensões, unidades)\n");
    fmt::print("        • você quer o tamanho no tipo para evitar alocação\n");
    fmt::print("      A técnica clássica de conter a explosão é extrair a parte\n");
    fmt::print("      que não depende do NTTP para uma base não-template.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_dimensoes();
    demo_string_nttp();
    demo_auto_nttp();
    demo_custo();
}

} // namespace demo_nttp
