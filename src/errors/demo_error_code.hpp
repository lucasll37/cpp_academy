// =============================================================================
//  src/errors/demo_error_code.hpp
//
//  std::error_code — erros como VALORES
//  ────────────────────────────────────
//  Nem todo erro merece uma exceção. Quando a falha é ESPERADA e frequente
//  (arquivo não existe, conexão recusada, entrada inválida), pagar o custo do
//  unwinding a cada ocorrência é desperdício — e obriga o chamador a montar
//  um try/catch em volta do caminho comum.
//
//  `std::error_code` é a resposta da biblioteca padrão: um par
//  (valor inteiro, categoria) que identifica um erro de forma extensível,
//  sem alocar, sem lançar e comparável entre bibliotecas diferentes.
//
//    error_code       → o que ACONTECEU, específico da plataforma/biblioteca
//    error_condition  → o que SIGNIFICA, portável
//
//  Tópicos:
//  • error_code, error_category e o modelo de extensão
//  • Uma categoria de erro customizada
//  • error_code vs error_condition — comparar através de plataformas
//  • std::system_error: a ponte de volta para exceções
//  • A API dupla (com e sem exceção) do std::filesystem
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <charconv>
#include <filesystem>
#include <string>
#include <system_error>
#include <type_traits>

namespace demo_error_code {

// ─────────────────────────────────────────────────────────────────────────────
//  1. error_code do sistema
//
//     A categoria `std::system_category()` mapeia os códigos do SO (errno no
//     Linux, GetLastError no Windows). `message()` devolve a descrição legível.
// ─────────────────────────────────────────────────────────────────────────────
void demo_system_error_code() {
    fmt::print("\n── 4.1 error_code do sistema ──\n");

    std::error_code ec = std::make_error_code(std::errc::no_such_file_or_directory);

    fmt::print("      value()            = {}\n", ec.value());
    fmt::print("      category().name()  = {}\n", ec.category().name());
    fmt::print("      message()          = {}\n", ec.message());

    // error_code converte para bool: `true` significa QUE HOUVE ERRO.
    // Repare que a polaridade é o contrário do habitual.
    fmt::print("      if (ec) → {}   (true = houve erro)\n", static_cast<bool>(ec));

    std::error_code ok;                        // default = sucesso
    fmt::print("      error_code{{}} default → {} (sem erro)\n", static_cast<bool>(ok));
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Uma categoria de erro customizada
//
//     Três passos:
//       (a) um enum class com os seus códigos
//       (b) uma categoria singleton que sabe traduzi-los em texto
//       (c) especializar std::is_error_code_enum para o enum converter sozinho
// ─────────────────────────────────────────────────────────────────────────────

// (a) Os códigos do seu domínio. 0 é reservado para "sucesso".
enum class ErroDeParse {
    ok = 0,
    token_inesperado,
    fim_prematuro,
    numero_invalido,
    profundidade_excedida,
};

// (b) A categoria: um singleton que traduz código → mensagem
class CategoriaDeParse : public std::error_category {
public:
    const char* name() const noexcept override { return "parse"; }

    std::string message(int ev) const override {
        switch (static_cast<ErroDeParse>(ev)) {
            case ErroDeParse::ok:                    return "sucesso";
            case ErroDeParse::token_inesperado:      return "token inesperado";
            case ErroDeParse::fim_prematuro:         return "fim prematuro da entrada";
            case ErroDeParse::numero_invalido:       return "número mal formado";
            case ErroDeParse::profundidade_excedida: return "aninhamento profundo demais";
        }
        return "erro de parse desconhecido";
    }

    // Mapeia os NOSSOS códigos para condições PORTÁVEIS da std.
    // Assim, código genérico consegue testar `ec == std::errc::invalid_argument`
    // sem conhecer a nossa categoria.
    std::error_condition default_error_condition(int ev) const noexcept override {
        switch (static_cast<ErroDeParse>(ev)) {
            case ErroDeParse::numero_invalido:
            case ErroDeParse::token_inesperado:
                return std::errc::invalid_argument;
            case ErroDeParse::profundidade_excedida:
                return std::errc::result_out_of_range;
            default:
                return std::error_condition(ev, *this);
        }
    }
};

inline const std::error_category& categoria_de_parse() noexcept {
    static CategoriaDeParse instancia;      // singleton com inicialização thread-safe
    return instancia;
}

// A função que a especialização abaixo habilita (encontrada por ADL)
inline std::error_code make_error_code(ErroDeParse e) noexcept {
    return {static_cast<int>(e), categoria_de_parse()};
}

} // namespace demo_error_code

// (c) Registra o enum como "enum de error_code": habilita a conversão implícita
//     de ErroDeParse para std::error_code e a comparação direta.
template <>
struct std::is_error_code_enum<demo_error_code::ErroDeParse> : std::true_type {};

namespace demo_error_code {

// Uma função que reporta erro por VALOR, sem lançar.
// Repare no `std::from_chars` em vez de `std::stoi`: stoi LANÇA em overflow,
// o que quebraria a promessa do `noexcept` e chamaria std::terminate.
std::error_code analisar(const std::string& entrada, int& resultado) noexcept {
    if (entrada.empty())                    return ErroDeParse::fim_prematuro;
    if (entrada.find_first_not_of("0123456789") != std::string::npos)
        return ErroDeParse::numero_invalido;

    const auto [ptr, ec] = std::from_chars(
        entrada.data(), entrada.data() + entrada.size(), resultado);
    if (ec == std::errc::result_out_of_range) return ErroDeParse::profundidade_excedida;
    if (ec != std::errc{} || ptr != entrada.data() + entrada.size())
        return ErroDeParse::numero_invalido;

    return {};                              // sucesso: error_code default
}

void demo_categoria_customizada() {
    fmt::print("\n── 4.2 Categoria de erro customizada ──\n");

    for (const std::string entrada : {"123", "12a", ""}) {
        int valor = 0;
        std::error_code ec = analisar(entrada, valor);

        if (ec) {
            fmt::print("      '{:<4}' → erro [{}:{}] {}\n",
                       entrada, ec.category().name(), ec.value(), ec.message());
        } else {
            fmt::print("      '{:<4}' → ok, valor = {}\n", entrada, valor);
        }
    }

    fmt::print("      Nenhuma exceção foi lançada e nenhuma pilha foi desenrolada.\n");
    fmt::print("      A função é `noexcept` e o chamador não precisa de try/catch.\n");
    fmt::print("      (o próprio error_code não aloca — só `message()` constrói\n");
    fmt::print("       uma std::string, e só quando você pede a mensagem)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. error_code vs error_condition
//
//     error_code      = o erro CONCRETO ("ENOENT da categoria system")
//     error_condition = a CLASSE de erro ("não existe tal arquivo", portável)
//
//     A comparação `ec == cond` usa default_error_condition() — por isso o
//     nosso `numero_invalido` consegue casar com `std::errc::invalid_argument`.
// ─────────────────────────────────────────────────────────────────────────────
void demo_condition() {
    fmt::print("\n── 4.3 error_code vs error_condition ──\n");

    std::error_code nosso = ErroDeParse::numero_invalido;

    fmt::print("      nosso.category()  = {}\n", nosso.category().name());
    fmt::print("      nosso.message()   = {}\n", nosso.message());

    // Código genérico, que nunca ouviu falar da nossa categoria:
    fmt::print("      nosso == std::errc::invalid_argument    ? {}\n",
               nosso == std::errc::invalid_argument);
    fmt::print("      nosso == std::errc::no_such_file_or_dir ? {}\n",
               nosso == std::errc::no_such_file_or_directory);

    std::error_code do_sistema = std::make_error_code(std::errc::permission_denied);
    fmt::print("      do_sistema == std::errc::permission_denied ? {}\n",
               do_sistema == std::errc::permission_denied);

    fmt::print("      → categorias diferentes conversam através das CONDITIONS.\n");
    fmt::print("        É assim que Asio, filesystem e o seu código coexistem.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. std::system_error — a ponte de volta
//
//     Quando o erro por valor chega a uma fronteira onde não pode mais ser
//     ignorado, `std::system_error` o transforma em exceção sem perder nada:
//     ele CARREGA o error_code original.
// ─────────────────────────────────────────────────────────────────────────────
void demo_system_error() {
    fmt::print("\n── 4.4 std::system_error — de valor para exceção ──\n");

    try {
        std::error_code ec = ErroDeParse::profundidade_excedida;
        if (ec) throw std::system_error(ec, "ao analisar config.json");
    }
    catch (const std::system_error& e) {
        fmt::print("      what()   = {}\n", e.what());
        fmt::print("      code()   = {}:{}\n",
                   e.code().category().name(), e.code().value());
        fmt::print("      ↑ o error_code sobreviveu à conversão\n");
    }

    fmt::print("      Padrão comum em bibliotecas: oferecer as DUAS APIs —\n");
    fmt::print("      uma que devolve error_code e outra que lança.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. A API dupla do std::filesystem
//
//     Toda função do <filesystem> tem duas assinaturas: uma que lança
//     filesystem_error e outra que recebe `std::error_code&` e não lança.
//     Escolha pela FREQUÊNCIA esperada do erro.
// ─────────────────────────────────────────────────────────────────────────────
void demo_filesystem() {
    fmt::print("\n── 4.5 A API dupla do std::filesystem ──\n");

    namespace fs = std::filesystem;
    const fs::path inexistente = "/caminho/que/nao/existe/arquivo.txt";

    // (a) Versão que NÃO lança — para o caso esperado
    std::error_code ec;
    auto tamanho = fs::file_size(inexistente, ec);
    if (ec) {
        fmt::print("      [sem exceção] falhou: {} (tamanho devolvido = {})\n",
                   ec.message(), tamanho);
    }

    // (b) Versão que lança — para o caso realmente excepcional
    try {
        auto t = fs::file_size(inexistente);
        fmt::print("      inalcançável: {}\n", t);
    }
    catch (const fs::filesystem_error& e) {
        fmt::print("      [com exceção] {}\n", e.what());
        fmt::print("                    path1() = {}\n", e.path1().string());
        fmt::print("                    code()  = {}\n", e.code().message());
    }

    fmt::print("\n      | o erro é...        | use                      |\n");
    fmt::print("      |--------------------|--------------------------|\n");
    fmt::print("      | esperado, frequente| error_code / expected     |\n");
    fmt::print("      | excepcional, raro  | exceção                   |\n");
    fmt::print("      | bug do chamador    | assert / logic_error      |\n");
    fmt::print("      | irrecuperável      | std::terminate / abort    |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_system_error_code();
    demo_categoria_customizada();
    demo_condition();
    demo_system_error();
    demo_filesystem();
}

} // namespace demo_error_code
