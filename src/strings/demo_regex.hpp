// =============================================================================
//  src/strings/demo_regex.hpp
//
//  std::regex — a API completa e o custo que ninguém avisa
//  ───────────────────────────────────────────────────────
//  `std::regex` chegou no C++11 e é, com folga, a parte mais criticada da
//  biblioteca padrão. A API é razoável; a IMPLEMENTAÇÃO é notoriamente lenta —
//  ordens de grandeza atrás de PCRE, RE2 ou até de um parser escrito à mão.
//
//  Este demo ensina a usar, e é explícito sobre quando NÃO usar.
//
//  Tópicos:
//  • match vs search vs iterate — a distinção que confunde
//  • Grupos de captura e smatch
//  • regex_replace e o formato de substituição
//  • As gramáticas disponíveis e os flags
//  • ⚠ O custo, medido — e as alternativas
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace demo_regex {

// ─────────────────────────────────────────────────────────────────────────────
//  1. match vs search — a distinção que mais confunde
// ─────────────────────────────────────────────────────────────────────────────
void demo_match_vs_search() {
    fmt::print("\n── 5.1 match vs search ──\n");

    const std::regex numero{R"(\d+)"};
    const std::string texto = "pedido 42 entregue";

    fmt::print("      texto: '{}'\n", texto);
    fmt::print("      padrão: \\d+\n\n");

    fmt::print("      regex_match  → {}  (exige que TODA a string case)\n",
               std::regex_match(texto, numero));
    fmt::print("      regex_search → {}  (procura em qualquer posição)\n",
               std::regex_search(texto, numero));

    fmt::print("\n      regex_match(\"42\", \\d+) → {}  (agora a string inteira casa)\n",
               std::regex_match(std::string{"42"}, numero));

    fmt::print("\n      É o erro nº 1 com std::regex: usar `match` esperando o\n");
    fmt::print("      comportamento de `search`. Em Python, `re.match` casa no\n");
    fmt::print("      INÍCIO; aqui, `regex_match` exige a string TODA.\n");

    fmt::print("\n      | função        | casa                    |\n");
    fmt::print("      |---------------|-------------------------|\n");
    fmt::print("      | regex_match   | a string INTEIRA        |\n");
    fmt::print("      | regex_search  | a primeira ocorrência   |\n");
    fmt::print("      | sregex_iterator| TODAS as ocorrências   |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Grupos de captura
// ─────────────────────────────────────────────────────────────────────────────
void demo_capturas() {
    fmt::print("\n── 5.2 Grupos de captura ──\n");

    // Um parser de data ISO, com três grupos
    const std::regex data{R"((\d{4})-(\d{2})-(\d{2}))"};
    const std::string entrada = "evento em 2026-08-25 às 14h";

    std::smatch m;
    if (std::regex_search(entrada, m, data)) {
        fmt::print("      entrada : '{}'\n", entrada);
        fmt::print("      m[0] (tudo)  = '{}'  posição {}, tamanho {}\n",
                   m[0].str(), m.position(0), m.length(0));
        fmt::print("      m[1] (ano)   = '{}'\n", m[1].str());
        fmt::print("      m[2] (mês)   = '{}'\n", m[2].str());
        fmt::print("      m[3] (dia)   = '{}'\n", m[3].str());
        fmt::print("      prefix       = '{}'\n", m.prefix().str());
        fmt::print("      suffix       = '{}'\n", m.suffix().str());
    }

    fmt::print("\n      ⚠  `std::smatch` guarda ITERADORES para a string original.\n");
    fmt::print("         Se ela morrer, o smatch fica pendurado — e o caso mais\n");
    fmt::print("         comum é fatal:\n");
    fmt::print("           std::smatch m;\n");
    fmt::print("           std::regex_search(obterTexto(), m, re);  // ✗ temporário!\n");
    fmt::print("           m[1].str();                               // use-after-free\n");
    fmt::print("         Por isso o padrão DELETOU a sobrecarga que recebe\n");
    fmt::print("         `const string&&` — o código acima nem compila.\n");

    // Todas as ocorrências, com sregex_iterator
    const std::string log = "erro 404, erro 500, erro 403";
    const std::regex  erro{R"(erro (\d{3}))"};

    fmt::print("\n      todas as ocorrências (sregex_iterator):\n");
    for (auto it = std::sregex_iterator{log.begin(), log.end(), erro};
         it != std::sregex_iterator{}; ++it) {
        fmt::print("        código {} na posição {}\n", (*it)[1].str(), it->position());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. regex_replace
// ─────────────────────────────────────────────────────────────────────────────
void demo_replace() {
    fmt::print("\n── 5.3 regex_replace ──\n");

    const std::string texto = "contato: ada@exemplo.com e grace@teste.org";
    const std::regex  email{R"((\w+)@([\w.]+))"};

    fmt::print("      original: '{}'\n", texto);

    // $1, $2 referenciam os grupos; $& é o match inteiro
    fmt::print("      mascarar   : '{}'\n",
               std::regex_replace(texto, email, "$1@***"));
    fmt::print("      inverter   : '{}'\n",
               std::regex_replace(texto, email, "$2 <- $1"));
    fmt::print("      destacar   : '{}'\n",
               std::regex_replace(texto, email, "[$&]"));

    // format_no_copy: devolve SÓ as substituições, sem o texto ao redor
    fmt::print("      só os matches: '{}'\n",
               std::regex_replace(texto, email, "$1;",
                                  std::regex_constants::format_no_copy));

    fmt::print("\n      | no formato | significa            |\n");
    fmt::print("      |------------|----------------------|\n");
    fmt::print("      | $1 .. $9   | grupo capturado      |\n");
    fmt::print("      | $&         | o match inteiro      |\n");
    fmt::print("      | $`         | o texto ANTES        |\n");
    fmt::print("      | $'         | o texto DEPOIS       |\n");
    fmt::print("      | $$         | um '$' literal       |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Gramáticas e flags
// ─────────────────────────────────────────────────────────────────────────────
void demo_gramaticas() {
    fmt::print("\n── 5.4 Gramáticas e flags ──\n");

    const std::string texto = "Erro Grave";

    const std::regex sensivel{"erro"};
    const std::regex insensivel{"erro", std::regex_constants::icase};

    fmt::print("      search(\"{}\", /erro/)        = {}\n",
               texto, std::regex_search(texto, sensivel));
    fmt::print("      search(\"{}\", /erro/i)       = {}\n",
               texto, std::regex_search(texto, insensivel));

    fmt::print("\n      gramáticas disponíveis:\n");
    fmt::print("        ECMAScript  (padrão)  — a do JavaScript, a mais completa\n");
    fmt::print("        basic / extended      — POSIX BRE / ERE\n");
    fmt::print("        awk / grep / egrep    — compatibilidade com as ferramentas\n");

    fmt::print("\n      flags de construção úteis:\n");
    fmt::print("        icase       — ignora maiúsculas\n");
    fmt::print("        multiline   — ^ e $ casam em cada linha (C++17)\n");
    fmt::print("        nosubs      — não guarda grupos → mais rápido\n");
    fmt::print("        optimize    — gasta mais tempo construindo p/ casar rápido\n");

    fmt::print("\n      ⚠  SEMPRE use raw strings para o padrão:\n");
    fmt::print("           std::regex re{{\"\\\\\\\\d+\"}};   // escape duplo, ilegível\n");
    fmt::print("           std::regex re{{R\"(\\d+)\"}};  // ✓ claro\n");
    fmt::print("         Sem raw string, `\\d` precisa virar `\\\\d` porque o\n");
    fmt::print("         compilador processa os escapes ANTES da regex.\n");

    fmt::print("\n      ⚠  construir um `std::regex` é CARO (compila a expressão).\n");
    fmt::print("         Nunca construa dentro de um laço:\n");
    fmt::print("           for (...) {{ std::regex re{{padrao}}; ... }}   // ✗\n");
    fmt::print("           static const std::regex re{{padrao}};         // ✓\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. ⚠ O custo, medido — e as alternativas
// ─────────────────────────────────────────────────────────────────────────────
void demo_custo() {
    fmt::print("\n── 5.5 ⚠ O custo, medido ──\n");

    constexpr int N = 20'000;
    const std::string linha = "2026-08-25 14:30:00 ERRO modulo=rede codigo=500";

    using clock = std::chrono::steady_clock;
    auto cronometrar = [](auto f) {
        const auto t0 = clock::now();
        const volatile long r = f();
        (void)r;
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   clock::now() - t0).count();
    };

    // (a) regex construída UMA vez, fora do laço
    static const std::regex re_codigo{R"(codigo=(\d+))"};
    const auto t_regex = cronometrar([&] {
        long soma = 0;
        for (int i = 0; i < N; ++i) {
            std::smatch m;
            if (std::regex_search(linha, m, re_codigo)) soma += std::stoi(m[1].str());
        }
        return soma;
    });

    // (b) regex construída DENTRO do laço — o erro comum
    const auto t_regex_no_laco = cronometrar([&] {
        long soma = 0;
        for (int i = 0; i < N / 10; ++i) {        // 10× menos iterações!
            const std::regex re{R"(codigo=(\d+))"};
            std::smatch m;
            if (std::regex_search(linha, m, re)) soma += std::stoi(m[1].str());
        }
        return soma;
    });

    // (c) find + from_chars — o parser escrito à mão
    const auto t_manual = cronometrar([&] {
        long soma = 0;
        for (int i = 0; i < N; ++i) {
            const auto pos = linha.find("codigo=");
            if (pos != std::string::npos) {
                const char* p = linha.data() + pos + 7;
                int v = 0;
                std::from_chars(p, linha.data() + linha.size(), v);
                soma += v;
            }
        }
        return soma;
    });

    fmt::print("      extraindo 'codigo=(\\d+)' de uma linha de log:\n");
    fmt::print("        find + from_chars       : {:>5} ms  ({} iterações)\n",
               t_manual, N);
    fmt::print("        regex fora do laço      : {:>5} ms  ({} iterações)\n",
               t_regex, N);
    fmt::print("        regex DENTRO do laço    : {:>5} ms  ({} iterações — 10× MENOS!)\n",
               t_regex_no_laco, N / 10);

    fmt::print("\n      Duas lições distintas:\n");
    fmt::print("        1. construir a regex no laço domina tudo. Note que a\n");
    fmt::print("           terceira linha fez 10× MENOS trabalho e ainda assim\n");
    fmt::print("           levou mais tempo.\n");
    fmt::print("        2. mesmo com a regex pré-construída, o `find` +\n");
    fmt::print("           `from_chars` é muito mais rápido — para um padrão\n");
    fmt::print("           FIXO e simples, a regex não se paga.\n");

    fmt::print("\n      Quando `std::regex` é a escolha certa:\n");
    fmt::print("        • o padrão vem do USUÁRIO (config, entrada) — aí não há\n");
    fmt::print("          como escrever um parser específico\n");
    fmt::print("        • a expressão é complexa e o código roda uma vez\n");
    fmt::print("        • legibilidade importa mais que velocidade\n");

    fmt::print("\n      Quando NÃO usar:\n");
    fmt::print("        • laço quente com padrão FIXO → find/starts_with/from_chars\n");
    fmt::print("        • parsing de formato conhecido → escreva o parser\n");
    fmt::print("          (ver src/parser/, com Flex e Bison)\n");
    fmt::print("        • validação simples → `all_of` com um predicado\n");
    fmt::print("        • você precisa de performance real → RE2, PCRE2, CTRE\n");
    fmt::print("          (o CTRE compila a regex em COMPILAÇÃO, com templates)\n");

    fmt::print("\n      ⚠  há também um risco de SEGURANÇA: `std::regex` usa\n");
    fmt::print("         backtracking, então um padrão como (a+)+$ contra uma\n");
    fmt::print("         entrada longa leva tempo EXPONENCIAL. É o ataque ReDoS.\n");
    fmt::print("         Nunca aplique regex de terceiros a entrada de terceiros.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_match_vs_search();
    demo_capturas();
    demo_replace();
    demo_gramaticas();
    demo_custo();
}

} // namespace demo_regex
