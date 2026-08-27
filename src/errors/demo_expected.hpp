// =============================================================================
//  src/errors/demo_expected.hpp
//
//  std::expected<T, E> — C++23
//  ───────────────────────────
//  `optional<T>` diz "não tem valor" mas não diz POR QUÊ. `error_code` diz o
//  porquê mas não carrega o valor. `expected<T,E>` faz as duas coisas: contém
//  OU um T (sucesso) OU um E (erro), e o compilador força você a decidir qual.
//
//  É o mesmo conceito de `Result<T,E>` do Rust e `Either` do Haskell.
//
//    ⚠ Requer C++23. Disponível no GCC 13 (libstdc++ 13). Este subprojeto
//      declara `override_options: ['cpp_std=c++23']` no seu meson.build,
//      sem alterar o padrão C++20 da raiz.
//
//  Tópicos:
//  • Construir, testar e extrair
//  • O erro tipado — mais que um int
//  • Encadeamento monádico: and_then, transform, or_else
//  • Comparação lado a lado com exceção, optional e error_code
//  • Quando ainda vale usar exceção
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <charconv>
#include <expected>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace demo_expected {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O erro TIPADO
//
//     A grande diferença para `optional` e para um `int` de retorno: o erro é
//     um tipo do SEU domínio, com os campos que você precisar.
// ─────────────────────────────────────────────────────────────────────────────
enum class Falha {
    vazio,
    caractere_invalido,
    fora_do_intervalo,
    negativo_nao_permitido,
};

constexpr std::string_view descrever(Falha f) noexcept {
    switch (f) {
        case Falha::vazio:                  return "entrada vazia";
        case Falha::caractere_invalido:     return "caractere não numérico";
        case Falha::fora_do_intervalo:      return "número fora do intervalo de int";
        case Falha::negativo_nao_permitido: return "negativo não permitido aqui";
    }
    return "falha desconhecida";
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Uma função que devolve expected
//
//     `std::unexpected{e}` é o construtor do lado do erro. Sem ele, o
//     compilador não saberia se `return e;` quer dizer sucesso ou falha
//     quando T e E fossem conversíveis entre si.
// ─────────────────────────────────────────────────────────────────────────────
std::expected<int, Falha> para_inteiro(std::string_view s) noexcept {
    if (s.empty()) return std::unexpected{Falha::vazio};

    int valor = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), valor);

    if (ec == std::errc::result_out_of_range) return std::unexpected{Falha::fora_do_intervalo};
    if (ec != std::errc{} || ptr != s.data() + s.size())
        return std::unexpected{Falha::caractere_invalido};

    return valor;                    // sucesso: conversão implícita para expected
}

void demo_basico() {
    fmt::print("\n── 5.1 Construir, testar, extrair ──\n");

    for (std::string_view entrada : {"42", "-7", "abc", "", "99999999999999"}) {
        auto r = para_inteiro(entrada);

        // Conversão para bool: `true` significa SUCESSO.
        // (polaridade oposta à do error_code — cuidado ao alternar entre os dois)
        if (r) {
            fmt::print("      '{:<15}' → ok    : {}\n", entrada, *r);
        } else {
            fmt::print("      '{:<15}' → falha : {}\n", entrada, descrever(r.error()));
        }
    }

    // `value_or` para um default; `value()` LANÇA std::bad_expected_access
    auto bom  = para_inteiro("100");
    auto ruim = para_inteiro("xyz");

    fmt::print("      value_or(0) em sucesso  = {}\n", bom.value_or(0));
    fmt::print("      value_or(0) em falha    = {}\n", ruim.value_or(0));

    try {
        (void)ruim.value();          // acesso ao valor de um expected em erro
    } catch (const std::bad_expected_access<Falha>& e) {
        fmt::print("      .value() em falha lança bad_expected_access (erro={})\n",
                   descrever(e.error()));
    }

    fmt::print("      Use *r  quando JÁ testou (sem verificação, como optional).\n");
    fmt::print("      Use .value() quando quiser a exceção como rede de segurança.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Encadeamento monádico
//
//     O ganho real do expected. Sem ele, cada passo vira um `if (!r) return r;`
//     e o código afoga em verificações. Com ele, o caminho de erro é implícito:
//     se qualquer etapa falhar, as seguintes são PULADAS e o erro é propagado.
//
//       and_then(f)  — f recebe T, devolve expected<U,E>   (encadeia)
//       transform(f) — f recebe T, devolve U               (mapeia o sucesso)
//       or_else(f)   — f recebe E, devolve expected<T,G>   (recupera)
//       transform_error(f) — mapeia o ERRO                 (traduz)
// ─────────────────────────────────────────────────────────────────────────────
std::expected<int, Falha> exigir_positivo(int v) noexcept {
    if (v < 0) return std::unexpected{Falha::negativo_nao_permitido};
    return v;
}

void demo_encadeamento() {
    fmt::print("\n── 5.2 Encadeamento monádico ──\n");

    auto pipeline = [](std::string_view entrada) {
        return para_inteiro(entrada)              // expected<int, Falha>
            .and_then(exigir_positivo)            // só roda se deu certo
            .transform([](int v) { return v * 2; })   // mapeia só o sucesso
            .transform([](int v) { return fmt::format("resultado={}", v); });
    };

    for (std::string_view entrada : {"21", "-5", "abc"}) {
        auto r = pipeline(entrada);
        if (r) fmt::print("      '{:<5}' → {}\n", entrada, *r);
        else   fmt::print("      '{:<5}' → falhou: {}\n", entrada, descrever(r.error()));
    }

    fmt::print("      Repare: nenhum `if` no meio do pipeline. A primeira falha\n");
    fmt::print("      curto-circuita todas as etapas seguintes.\n");

    // or_else recupera de uma falha específica
    auto com_fallback = para_inteiro("inválido")
        .or_else([](Falha f) -> std::expected<int, Falha> {
            fmt::print("      or_else viu '{}' e devolveu o default\n", descrever(f));
            return 0;
        });
    fmt::print("      após or_else: {}\n", *com_fallback);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Comparação lado a lado
//
//     A mesma operação, escrita das quatro formas.
// ─────────────────────────────────────────────────────────────────────────────
std::optional<int> versao_optional(std::string_view s) {
    int v = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{}) return std::nullopt;      // ← perdeu o PORQUÊ
    return v;
}

int versao_excecao(std::string_view s) {
    int v = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{}) throw std::invalid_argument(std::string("inválido: ") + std::string(s));
    return v;
}

bool versao_out_param(std::string_view s, int& saida) noexcept {
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), saida);
    return ec == std::errc{};                        // ← saída não inicializada em falha
}

void demo_comparacao() {
    fmt::print("\n── 5.3 As quatro formas, lado a lado ──\n");

    const std::string_view entrada = "abc";

    // (a) optional — simples, mas o motivo do erro se perde
    if (auto r = versao_optional(entrada); !r)
        fmt::print("      optional     : falhou (e não sei por quê)\n");

    // (b) exceção — carrega contexto rico, mas custa caro e é fácil de ignorar
    try { (void)versao_excecao(entrada); }
    catch (const std::exception& e) {
        fmt::print("      exceção      : {}\n", e.what());
    }

    // (c) out-param + bool — a forma C. Fácil de esquecer de checar.
    int saida = -1;
    if (!versao_out_param(entrada, saida))
        fmt::print("      out-param    : falhou (e `saida` está em estado duvidoso)\n");

    // (d) expected — o motivo vem junto, sem exceção, e não dá para ignorar
    if (auto r = para_inteiro(entrada); !r)
        fmt::print("      expected     : {}\n", descrever(r.error()));

    fmt::print("\n      | forma       | motivo do erro | custo  | dá p/ ignorar |\n");
    fmt::print("      |-------------|----------------|--------|---------------|\n");
    fmt::print("      | optional    | não            | zero   | sim           |\n");
    fmt::print("      | exceção     | sim, rico      | alto   | não           |\n");
    fmt::print("      | out-param   | via bool/enum  | zero   | sim           |\n");
    fmt::print("      | error_code  | sim, extensível| zero   | sim           |\n");
    fmt::print("      | expected    | sim, TIPADO    | zero   | sim*          |\n");
    fmt::print("      * expected não é [[nodiscard]] por si só — ver 5.5\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. expected<void, E> — quando não há valor a devolver
// ─────────────────────────────────────────────────────────────────────────────
std::expected<void, Falha> validar_todos(const std::vector<std::string>& entradas) noexcept {
    for (const auto& e : entradas) {
        auto r = para_inteiro(e);
        if (!r) return std::unexpected{r.error()};
    }
    return {};                          // sucesso "vazio"
}

void demo_expected_void() {
    fmt::print("\n── 5.4 expected<void, E> ──\n");

    if (auto r = validar_todos({"1", "2", "3"}); r)
        fmt::print("      todos válidos\n");

    if (auto r = validar_todos({"1", "dois", "3"}); !r)
        fmt::print("      falhou: {}\n", descrever(r.error()));

    fmt::print("      Substitui o `bool` de retorno sem perder o motivo.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Quando ainda usar exceção
//
//     `expected` não aposenta as exceções. Ele resolve o caso do erro
//     ESPERADO e LOCAL. Exceções continuam melhores quando:
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando_usar_cada_um() {
    fmt::print("\n── 5.5 Escolhendo a ferramenta ──\n");

    fmt::print("      Use `expected` quando:\n");
    fmt::print("        • a falha é esperada e frequente\n");
    fmt::print("        • o chamador imediato sabe o que fazer\n");
    fmt::print("        • você está num caminho quente (parser, loop de rede)\n");
    fmt::print("        • exceções estão desabilitadas (-fno-exceptions)\n");

    fmt::print("\n      Use exceção quando:\n");
    fmt::print("        • o erro precisa atravessar MUITAS camadas intactas\n");
    fmt::print("          (propagar expected exige mudar toda assinatura no caminho)\n");
    fmt::print("        • falha em construtor — não há valor de retorno\n");
    fmt::print("        • falha em operador sobrecarregado — a assinatura é fixa\n");
    fmt::print("        • o erro é realmente excepcional e raro\n");

    fmt::print("\n      ⚠  `std::expected` NÃO é [[nodiscard]] no padrão — ignorar o\n");
    fmt::print("      retorno compila sem um pio. Marque VOCÊ as suas funções:\n");
    fmt::print("        [[nodiscard]] std::expected<int,Falha> para_inteiro(...);\n");
    fmt::print("      Aí sim o compilador cobra. Vale o mesmo para error_code.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_encadeamento();
    demo_comparacao();
    demo_expected_void();
    demo_quando_usar_cada_um();
}

} // namespace demo_expected
