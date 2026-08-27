// =============================================================================
//  src/cpp23/demo_biblioteca.hpp
//
//  As novidades de BIBLIOTECA do C++23 que o GCC 13 tem
//  ────────────────────────────────────────────────────
//  `std::expected` já tem tratamento completo em src/errors/demo_expected.hpp,
//  e `std::byteswap` em src/bits/demo_endianness.hpp. Este demo cobre o resto:
//  as adições pequenas que passam despercebidas e resolvem irritações antigas.
//
//  Tópicos:
//  • std::to_underlying — o fim do static_cast em enums
//  • std::unreachable — e a diferença para [[assume]]
//  • views::zip e zip_transform
//  • As operações monádicas que chegaram no optional
//  • std::invoke_r e outras adições menores
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <type_traits>
#include <version>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace demo_biblioteca {

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::to_underlying
//
//     Converter um `enum class` no seu tipo subjacente exigia repetir o tipo:
//       static_cast<std::underlying_type_t<Cor>>(c)
//     — verboso, e errado se o tipo subjacente mudar depois.
// ─────────────────────────────────────────────────────────────────────────────
enum class Codigo : std::uint16_t {
    Ok           = 200,
    NaoEncontrado = 404,
    ErroInterno  = 500,
};

enum class Nivel : std::int8_t { Debug = 0, Info = 1, Aviso = 2, Erro = 3 };

void demo_to_underlying() {
    fmt::print("\n── 3.1 std::to_underlying ──\n");

    const auto c = Codigo::NaoEncontrado;

    fmt::print("      antes:\n");
    fmt::print("        static_cast<std::underlying_type_t<Codigo>>(c)  → {}\n",
               static_cast<std::underlying_type_t<Codigo>>(c));
    fmt::print("      agora:\n");
    fmt::print("        std::to_underlying(c)                           → {}\n",
               std::to_underlying(c));

    fmt::print("\n      O ganho não é digitar menos — é não REPETIR o tipo.\n");
    fmt::print("      Se `Codigo` mudar de uint16_t para uint32_t, o\n");
    fmt::print("      `static_cast` explícito continua compilando e trunca;\n");
    fmt::print("      o `to_underlying` acompanha sozinho.\n");

    // O tipo devolvido acompanha a declaração do enum
    static_assert(std::is_same_v<decltype(std::to_underlying(Codigo::Ok)),
                                 std::uint16_t>);
    static_assert(std::is_same_v<decltype(std::to_underlying(Nivel::Erro)),
                                 std::int8_t>);
    fmt::print("\n      to_underlying(Codigo::Ok) é uint16_t ✓\n");
    fmt::print("      to_underlying(Nivel::Erro) é int8_t   ✓\n");

    // Uso típico: indexar uma tabela por enum
    constexpr const char* nomes[] = {"DEBUG", "INFO", "AVISO", "ERRO"};
    for (auto n : {Nivel::Debug, Nivel::Aviso, Nivel::Erro}) {
        fmt::print("        Nivel {} → '{}'\n", std::to_underlying(n),
                   nomes[std::to_underlying(n)]);
    }

    fmt::print("\n      ⚠  ele NÃO faz o caminho de volta. `int → enum class`\n");
    fmt::print("         continua exigindo `static_cast<Codigo>(404)`, e continua\n");
    fmt::print("         sem validar que o valor é um enumerador existente.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. std::unreachable
//
//     Marca um ponto do código que NUNCA é alcançado. O compilador remove o
//     caminho e pode otimizar o resto — e se você errar, é UB.
// ─────────────────────────────────────────────────────────────────────────────
std::string_view descrever(Nivel n) {
    switch (n) {
        case Nivel::Debug: return "depuração";
        case Nivel::Info:  return "informativo";
        case Nivel::Aviso: return "aviso";
        case Nivel::Erro:  return "erro";
    }
    // Todos os enumeradores foram tratados. Sem esta linha, o GCC avisa
    // "control reaches end of non-void function" — porque um `enum class` pode
    // guardar valores fora da lista (ex.: `static_cast<Nivel>(99)`).
    std::unreachable();
}

void demo_unreachable() {
    fmt::print("\n── 3.2 std::unreachable ──\n");

    for (auto n : {Nivel::Debug, Nivel::Info, Nivel::Aviso, Nivel::Erro}) {
        fmt::print("      Nivel {} → '{}'\n", std::to_underlying(n), descrever(n));
    }

    fmt::print("\n      O problema que ele resolve:\n");
    fmt::print("        um switch que cobre TODOS os enumeradores ainda deixa o\n");
    fmt::print("        compilador avisar \"control reaches end of non-void\n");
    fmt::print("        function\" — porque um `enum class` PODE guardar um valor\n");
    fmt::print("        fora da lista, via static_cast.\n");

    fmt::print("\n      As três saídas, e por que `unreachable` é a melhor:\n");
    fmt::print("        return {{}};          → esconde o bug com um valor falso\n");
    fmt::print("        throw ...;          → custa, e o caso é impossível\n");
    fmt::print("        std::unreachable(); → diz a VERDADE ao compilador\n");

    fmt::print("\n      ⚠  `unreachable` vs `[[assume]]`:\n");
    fmt::print("        std::unreachable()   → \"aqui nunca chega\"\n");
    fmt::print("        [[assume(cond)]]     → \"cond é verdadeira\"\n");
    fmt::print("        `unreachable()` é equivalente a `[[assume(false)]]`.\n");
    fmt::print("        Os dois são UB se a premissa estiver errada.\n");

    fmt::print("\n      Antes do C++23:\n");
    fmt::print("        __builtin_unreachable()   (GCC/Clang)\n");
    fmt::print("        __assume(0)               (MSVC)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. views::zip e zip_transform
//
//     Percorrer duas sequências em paralelo era o buraco mais visível de
//     <ranges> no C++20.
// ─────────────────────────────────────────────────────────────────────────────
void demo_zip() {
    fmt::print("\n── 3.3 views::zip ──\n");

    const std::vector<std::string> nomes{"Ada", "Grace", "Alan", "Linus"};
    const std::vector<int>         idades{36, 45, 41, 28};
    const std::vector<double>      salarios{9500, 11000, 10200, 8200};

    fmt::print("      zip de três sequências:\n");
    for (const auto& [nome, idade, salario] : std::views::zip(nomes, idades, salarios)) {
        fmt::print("        {:<7} {:>3} anos  R$ {:>8.2f}\n", nome, idade, salario);
    }

    // zip para no MENOR range
    const std::vector<int> curto{1, 2};
    fmt::print("\n      zip para no menor range:\n        ");
    for (const auto& [a, b] : std::views::zip(idades, curto)) {
        fmt::print("({},{}) ", a, b);
    }
    fmt::print("\n        → {} elementos, não {}\n",
               std::ranges::distance(std::views::zip(idades, curto)), idades.size());

    // zip_transform aplica uma função aos elementos correspondentes
    fmt::print("\n      zip_transform (soma elemento a elemento):\n        ");
    const std::vector<int> a{1, 2, 3}, b{10, 20, 30};
    for (int s : std::views::zip_transform(std::plus{}, a, b)) fmt::print("{} ", s);
    fmt::print("\n");

    fmt::print("\n      Antes do C++23, percorrer dois vetores em paralelo era:\n");
    fmt::print("        for (size_t i = 0; i < min(a.size(), b.size()); ++i)\n");
    fmt::print("      — com o `min` fácil de esquecer, e o índice sem significado.\n");

    fmt::print("\n      ⚠  o `zip` NÃO copia: ele referencia as sequências. Se\n");
    fmt::print("         uma delas morrer ou realocar, o view fica pendurado —\n");
    fmt::print("         é a mesma regra de src/ranges/demo_pipelines.hpp, 2.3.\n");

    // enumerate ainda NÃO está disponível
    fmt::print("\n      ⚠  `views::enumerate` (índice + valor) NÃO está neste\n");
    fmt::print("         GCC. O substituto com o que temos:\n");
    fmt::print("           views::zip(views::iota(0uz), v)\n        ");
    for (const auto& [i, nome] : std::views::zip(std::views::iota(0uz), nomes)) {
        fmt::print("[{}]{} ", i, nome);
    }
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. As monádicas do optional
//
//     Já usadas em src/vocabulary_types/demo_optional.hpp. Aqui vale registrar
//     que elas são C++23 — e que a versão do `optional` importa.
// ─────────────────────────────────────────────────────────────────────────────
void demo_optional_monadico() {
    fmt::print("\n── 3.4 As monádicas do optional ──\n");

    fmt::print("      __cpp_lib_optional = {}\n", __cpp_lib_optional);
    fmt::print("        >= 201606 → optional existe (C++17)\n");
    fmt::print("        >= 202106 → optional totalmente constexpr (C++23)\n");
    fmt::print("        >= 202110 → and_then / transform / or_else (C++23)\n");

    const auto parsear = [](std::string_view s) -> std::optional<int> {
        if (s == "42") return 42;
        return std::nullopt;
    };

    const auto pipeline = [&](std::string_view s) {
        return parsear(s)
            .transform([](int v) { return v * 2; })
            .and_then([](int v) -> std::optional<int> {
                return v > 50 ? std::optional{v} : std::nullopt;
            })
            .value_or(-1);
    };

    fmt::print("\n      parsear|transform(×2)|and_then(>50)|value_or(-1):\n");
    fmt::print("        \"42\"  → {}\n", pipeline("42"));
    fmt::print("        \"abc\" → {}\n", pipeline("abc"));

    fmt::print("\n      A mesma API existe em `std::expected` — a diferença é\n");
    fmt::print("      que o `expected` carrega o MOTIVO da falha.\n");
    fmt::print("      Ver src/errors/demo_expected.hpp e\n");
    fmt::print("      src/vocabulary_types/demo_optional.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Adições menores
// ─────────────────────────────────────────────────────────────────────────────
void demo_menores() {
    fmt::print("\n── 3.5 Adições menores ──\n");

    // std::invoke_r — invoke com tipo de retorno explícito
    const auto f = [] { return 3.99; };
    const int truncado = std::invoke_r<int>(f);
    fmt::print("      std::invoke_r<int>([]{{return 3.99;}}) = {}\n", truncado);
    fmt::print("        → converte o retorno para o tipo pedido\n");

    // string::contains — já usado em src/strings/
    const std::string s = "cpp_academy";
    fmt::print("\n      string::contains(\"academy\") = {}\n", s.contains("academy"));
    fmt::print("        antes: `s.find(\"x\") != std::string::npos`\n");

    // ranges::starts_with / ends_with — mais uma que a macro delata
    const std::vector<int> v{1, 2, 3, 4, 5};
#if defined(__cpp_lib_ranges_starts_ends_with)
    fmt::print("\n      ranges::starts_with(v, {{1,2}}) = {}\n",
               std::ranges::starts_with(v, std::vector<int>{1, 2}));
#else
    fmt::print("\n      ⚠  `ranges::starts_with` / `ends_with` NÃO estão neste\n");
    fmt::print("         GCC (__cpp_lib_ranges_starts_ends_with não definido).\n");
    fmt::print("         O equivalente com o que temos:\n");
    const std::vector<int> prefixo{1, 2};
    const bool comeca = v.size() >= prefixo.size() &&
                        std::equal(prefixo.begin(), prefixo.end(), v.begin());
    fmt::print("           std::equal(pref.begin(), pref.end(), v.begin()) = {}\n",
               comeca);
    fmt::print("         (`std::string` JÁ tem starts_with/ends_with desde o\n");
    fmt::print("          C++20 — a lacuna é só nos ranges genéricos)\n");
#endif

    // ranges::fold_left — o accumulate que faltava em <ranges>
#if defined(__cpp_lib_ranges_fold)
    // O `accumulate` que faltava em <ranges> — e que ESTE GCC já tem, ao
    // contrário do que a tabela do ROADMAP dizia antes de ser medida.
    fmt::print("\n      ranges::fold_left(v, 0, plus)  = {}\n",
               std::ranges::fold_left(v, 0, std::plus{}));
    fmt::print("      ranges::fold_left(v, 1, multiplies) = {}\n",
               std::ranges::fold_left(v, 1, std::multiplies{}));
    fmt::print("        → aceita o RANGE inteiro, e é constexpr-friendly.\n");
    fmt::print("        (src/ranges/demo_algorithms.hpp, 3.6 diz que os\n");
    fmt::print("         numéricos não têm versão em ranges — isso valia para\n");
    fmt::print("         `accumulate`; o `fold_left` é a versão nova deles)\n");
#else
    fmt::print("\n      `ranges::fold_left` não está neste toolchain.\n");
    fmt::print("        Use `std::accumulate(v.begin(), v.end(), 0)`.\n");
#endif

    fmt::print("\n      Outras que chegaram no C++23 e valem conhecer:\n");
    fmt::print("        std::move_only_function   — como std::function, mas\n");
    fmt::print("                                    aceita chamáveis move-only\n");
    fmt::print("        std::out_ptr / inout_ptr  — ponte com APIs C que\n");
    fmt::print("                                    devolvem por T**\n");
    fmt::print("        std::spanstream           — stream sobre um span, sem\n");
    fmt::print("                                    alocar (substitui strstream)\n");
    fmt::print("        views::chunk / slide      — janelas deslizantes\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_to_underlying();
    demo_unreachable();
    demo_zip();
    demo_optional_monadico();
    demo_menores();
}

} // namespace demo_biblioteca
