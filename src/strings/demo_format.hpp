// =============================================================================
//  src/strings/demo_format.hpp
//
//  std::format — formatação verificada em compilação
//  ─────────────────────────────────────────────────
//  O C++ tinha duas formas de formatar texto, e as duas eram ruins:
//
//    printf   — rápido, conciso, e SEM segurança de tipo. `printf("%d", 1.5)`
//               compila e produz lixo. `%s` com um std::string é UB.
//    iostreams— seguro, e verbosíssimo. Manipuladores são estado GLOBAL e
//               pegajoso: um `std::hex` esquecido contamina tudo depois.
//
//  `std::format` (C++20) é a terceira via: sintaxe de printf, segurança de
//  tipo dos iostreams, e — o ponto decisivo — a string de formato é validada
//  na COMPILAÇÃO.
//
//  ⚠ Este subprojeto usa `fmt::format` (a biblioteca que originou a proposta),
//    porque é o que já está no conanfile. A API é a mesma.
//
//  Tópicos:
//  • A validação em compilação — o que ela pega
//  • A mini-linguagem de especificação
//  • format_to e a versão sem alocação
//  • Um formatter customizado para o seu tipo
//  • O custo comparado com printf e ostringstream
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <chrono>
#include <cstdio>
#include <iterator>
#include <iomanip>
#include <sstream>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace demo_format {

// ─────────────────────────────────────────────────────────────────────────────
//  1. A validação em compilação
// ─────────────────────────────────────────────────────────────────────────────
void demo_validacao() {
    fmt::print("\n── 3.1 Validação em compilação ──\n");

    fmt::print("      printf, o problema histórico:\n");
    fmt::print("        printf(\"%d\", 1.5)        → compila, imprime lixo\n");
    fmt::print("        printf(\"%s\", std::string) → compila, é UB\n");
    fmt::print("        printf(\"%d %d\", 1)        → compila, lê a pilha\n");
    fmt::print("      (o GCC pega ALGUNS casos com -Wformat, mas só quando a\n");
    fmt::print("       string é literal e a função é conhecida)\n");

    fmt::print("\n      fmt/std::format:\n");
    fmt::print("        fmt::format(\"{{}}\", std::string{{}})  → ✓ funciona\n");
    fmt::print("        fmt::format(\"{{:d}}\", \"texto\")        → NÃO COMPILA\n");
    fmt::print("        fmt::format(\"{{}} {{}}\", 1)             → NÃO COMPILA\n");
    fmt::print("        fmt::format(\"{{\", 1)                  → NÃO COMPILA\n");

    fmt::print("\n      Como isso é possível: a string de formato é `consteval`.\n");
    fmt::print("      Ela é analisada em tempo de compilação, e um `throw` numa\n");
    fmt::print("      avaliação constexpr é ERRO DE BUILD.\n");
    fmt::print("      É o mecanismo de src/compile_time/demo_consteval.hpp, 2.3.\n");

    // Prova de que aceita qualquer tipo formatável, sem especificador
    const std::vector<int> v{1, 2, 3};
    fmt::print("\n      e ele conhece os tipos da STL:\n");
    fmt::print("        vector<int>{{1,2,3}} → {}\n", v);
    fmt::print("        pair<int,string>    → {}\n", std::pair{7, std::string{"sete"}});
    fmt::print("      (via <fmt/ranges.h>; em std::format isso chegou no C++23)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. A mini-linguagem de especificação
//
//     A forma geral:  {índice:preenchimento alinhamento sinal # 0 largura
//                            .precisão tipo}
// ─────────────────────────────────────────────────────────────────────────────
void demo_spec() {
    fmt::print("\n── 3.2 A mini-linguagem de especificação ──\n");

    fmt::print("      alinhamento e largura:\n");
    fmt::print("        '{{:<10}}' → '{:<10}'|\n", "esq");
    fmt::print("        '{{:>10}}' → '{:>10}'|\n", "dir");
    fmt::print("        '{{:^10}}' → '{:^10}'|\n", "centro");
    fmt::print("        '{{:*^10}}'→ '{:*^10}'|  (preenchimento customizado)\n", "abc");

    fmt::print("\n      números:\n");
    fmt::print("        '{{:d}}'    → {:d}\n",     42);
    fmt::print("        '{{:08.3f}}'→ {:08.3f}\n", 3.14159);
    fmt::print("        '{{:+}}'    → {:+}\n",     42);
    fmt::print("        '{{:.2e}}'  → {:.2e}\n",   123456.789);
    fmt::print("        '{{:g}}'    → {:g}     (o mais curto entre f e e)\n", 123456.789);

    // ⚠ Este demo tinha `{:.1%}` aqui. O `%` NÃO é especificador válido para
    //   double no fmt 10 (nem no std::format do C++20), e o erro foi pego na
    //   COMPILAÇÃO — que é justamente o que a seção 3.1 promete.
    //   A porcentagem se faz multiplicando antes:
    fmt::print("        porcentagem → {:.1f}%  (multiplique por 100 antes)\n",
               0.856 * 100);

    fmt::print("\n      bases:\n");
    fmt::print("        '{{:b}}'  → {:b}      (binário)\n",       42);
    fmt::print("        '{{:#b}}' → {:#b}   (com prefixo)\n",     42);
    fmt::print("        '{{:o}}'  → {:o}       (octal)\n",        42);
    fmt::print("        '{{:#x}}' → {:#x}     (hex com prefixo)\n", 255);
    fmt::print("        '{{:#X}}' → {:#X}     (maiúsculo)\n",     255);

    fmt::print("\n      índices explícitos e reuso de argumento:\n");
    fmt::print("        '{{0}} {{1}} {{0}}' → {0} {1} {0}\n", "A", "B");
    fmt::print("      ↑ com printf isso exigiria passar 'A' duas vezes.\n");

    fmt::print("\n      largura e precisão DINÂMICAS (o que printf faz com '*'):\n");
    const int largura = 12, precisao = 3;
    fmt::print("        '{{:{{}}.{{}}f}}' com (12,3) → '{:{}.{}f}'|\n",
               3.14159265, largura, precisao);

    fmt::print("\n      A comparação que resume:\n");
    fmt::print("        printf : \"%-10s|%08.3f\\n\"        conciso, inseguro\n");
    fmt::print("        iostream: std::setw(10) << std::left << ...  verboso, pegajoso\n");
    fmt::print("        format : \"{{:<10}}|{{:08.3f}}\"      conciso E seguro\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. format_to — escrever sem alocar uma string nova
//
//     `format` devolve uma `std::string` (aloca). `format_to` escreve num
//     ITERADOR de saída: um buffer existente, um stream, um vector.
// ─────────────────────────────────────────────────────────────────────────────
void demo_format_to() {
    fmt::print("\n── 3.3 format_to — sem alocar ──\n");

    // (a) Para um buffer na pilha, com limite — a versão segura
    char buffer[64];
    const auto resultado = fmt::format_to_n(buffer, sizeof(buffer) - 1,
                                            "id={} nome={}", 42, "teste");
    *resultado.out = '\0';
    fmt::print("      format_to_n em buffer[64] → '{}'\n", buffer);
    fmt::print("        escreveu {} chars (de {} que caberiam)\n",
               resultado.out - buffer, sizeof(buffer) - 1);

    // (b) Anexando a uma string existente, sem criar temporário
    std::string acumulado;
    acumulado.reserve(128);
    for (int i = 1; i <= 3; ++i) {
        fmt::format_to(std::back_inserter(acumulado), "[item {}] ", i);
    }
    fmt::print("      format_to(back_inserter) → '{}'\n", acumulado);
    fmt::print("        ↑ uma única alocação (o reserve), não três\n");

    // (c) fmt::memory_buffer — buffer com SBO próprio da fmt
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "{}-{}-{}", 2026, 8, 25);
    fmt::print("      fmt::memory_buffer → '{}'\n",
               std::string_view{buf.data(), buf.size()});
    fmt::print("        ↑ 500 bytes inline; só aloca acima disso\n");

    fmt::print("\n      | função         | destino            | aloca |\n");
    fmt::print("      |----------------|--------------------|-------|\n");
    fmt::print("      | format         | std::string nova   | sim   |\n");
    fmt::print("      | format_to      | qualquer iterador  | não*  |\n");
    fmt::print("      | format_to_n    | idem, com limite   | não   |\n");
    fmt::print("      | print          | stdout direto      | não   |\n");
    fmt::print("      * depende do destino: back_inserter num vector pode realocar\n");

    fmt::print("\n      `fmt::print` escreve direto no stdout, sem construir a\n");
    fmt::print("      string intermediária — é por isso que ele é usado em todo\n");
    fmt::print("      este repositório em vez de `std::cout << fmt::format(...)`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Um formatter customizado
//
//     Especializar `fmt::formatter<T>` (ou `std::formatter<T>`) ensina o
//     sistema a formatar o seu tipo — inclusive com especificadores próprios.
// ─────────────────────────────────────────────────────────────────────────────
struct Ponto {
    double x, y;
};

struct Temperatura {
    double celsius;
};

} // namespace demo_format

// ── Formatter simples: só imprime, sem especificadores ──────────────────────
template <>
struct fmt::formatter<demo_format::Ponto> {
    // parse: consome os especificadores entre ':' e '}'. Aqui, nenhum.
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw format_error("Ponto não aceita especificadores");
        }
        return it;
    }

    // format: escreve no iterador de saída
    auto format(const demo_format::Ponto& p, format_context& ctx) const {
        return fmt::format_to(ctx.out(), "({:.2f}, {:.2f})", p.x, p.y);
    }
};

// ── Formatter COM especificadores próprios: 'C' celsius, 'F' fahrenheit ─────
template <>
struct fmt::formatter<demo_format::Temperatura> {
    char unidade = 'C';

    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && (*it == 'C' || *it == 'F' || *it == 'K')) {
            unidade = *it++;
        }
        if (it != ctx.end() && *it != '}') {
            throw format_error("Temperatura aceita apenas C, F ou K");
        }
        return it;
    }

    auto format(const demo_format::Temperatura& t, format_context& ctx) const {
        switch (unidade) {
            case 'F': return fmt::format_to(ctx.out(), "{:.1f}°F", t.celsius * 9 / 5 + 32);
            case 'K': return fmt::format_to(ctx.out(), "{:.2f}K",  t.celsius + 273.15);
            default:  return fmt::format_to(ctx.out(), "{:.1f}°C", t.celsius);
        }
    }
};

namespace demo_format {

void demo_formatter() {
    fmt::print("\n── 3.4 Formatter customizado ──\n");

    const Ponto p{3.14159, 2.71828};
    fmt::print("      Ponto sem especificador : {}\n", p);
    fmt::print("      dentro de um container  : {}\n", std::vector<Ponto>{{1, 2}, {3, 4}});

    const Temperatura t{25.0};
    fmt::print("\n      Temperatura com especificadores próprios:\n");
    fmt::print("        '{{}}'   → {}\n",  t);
    fmt::print("        '{{:C}}' → {:C}\n", t);
    fmt::print("        '{{:F}}' → {:F}\n", t);
    fmt::print("        '{{:K}}' → {:K}\n", t);
    fmt::print("        '{{:X}}' → NÃO COMPILA (o parse lança em compilação)\n");

    fmt::print("\n      A estrutura de um formatter:\n");
    fmt::print("        template <> struct fmt::formatter<MeuTipo> {{\n");
    fmt::print("            constexpr auto parse(format_parse_context& ctx);\n");
    fmt::print("            auto format(const MeuTipo&, format_context&) const;\n");
    fmt::print("        }};\n");
    fmt::print("      O `parse` é `constexpr`: se ele lançar, o erro é de BUILD.\n");
    fmt::print("      É assim que `{{:X}}` acima vira erro de compilação.\n");

    fmt::print("\n      Atalho: herdar de um formatter existente reaproveita\n");
    fmt::print("      toda a mini-linguagem dele:\n");
    fmt::print("        struct formatter<Metros> : formatter<double> {{\n");
    fmt::print("            auto format(Metros m, auto& ctx) const {{\n");
    fmt::print("                return formatter<double>::format(m.valor, ctx);\n");
    fmt::print("            }}\n");
    fmt::print("        }};\n");
    fmt::print("      → `{{:8.2f}}` passa a funcionar em Metros de graça.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O custo, medido
// ─────────────────────────────────────────────────────────────────────────────
void demo_custo() {
    fmt::print("\n── 3.5 O custo, medido ──\n");

    constexpr int N = 100'000;
    using clock = std::chrono::steady_clock;

    auto cronometrar = [](auto f) {
        const auto t0 = clock::now();
        f();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   clock::now() - t0).count();
    };

    // (a) fmt::format — devolve uma std::string NOVA (aloca)
    const auto t_fmt = cronometrar([] {
        for (int i = 0; i < N; ++i) {
            const auto s = fmt::format("id={} valor={:.3f} nome={}", i, i * 1.5, "teste");
            const volatile auto n = s.size(); (void)n;
        }
    });

    // (b) fmt::format_to_n — escreve num buffer da pilha, como o snprintf.
    //     ESTA é a comparação justa contra o snprintf.
    const auto t_format_to = cronometrar([] {
        char buf[128];
        for (int i = 0; i < N; ++i) {
            const auto r = fmt::format_to_n(buf, sizeof(buf) - 1,
                                            "id={} valor={:.3f} nome={}", i, i * 1.5, "teste");
            const volatile auto n = r.size; (void)n;
        }
    });

    // (c) snprintf — buffer na pilha, sem segurança de tipo
    const auto t_sprintf = cronometrar([] {
        char buf[128];
        for (int i = 0; i < N; ++i) {
            std::snprintf(buf, sizeof(buf), "id=%d valor=%.3f nome=%s", i, i * 1.5, "teste");
            const volatile auto n = std::char_traits<char>::length(buf); (void)n;
        }
    });

    // (d) ostringstream — constrói um objeto de stream por iteração
    const auto t_sstream = cronometrar([] {
        for (int i = 0; i < N; ++i) {
            std::ostringstream oss;
            oss << "id=" << i << " valor=" << std::fixed << std::setprecision(3)
                << (i * 1.5) << " nome=" << "teste";
            const auto s = oss.str();
            const volatile auto n = s.size(); (void)n;
        }
    });

    fmt::print("      {} formatações da mesma linha:\n", N);
    fmt::print("        snprintf         : {:>5} ms  (buffer na pilha)\n", t_sprintf);
    fmt::print("        fmt::format_to_n : {:>5} ms  (buffer na pilha)\n", t_format_to);
    fmt::print("        ostringstream    : {:>5} ms  (string nova por iteração)\n", t_sstream);
    fmt::print("        fmt::format      : {:>5} ms  (string nova por iteração)\n", t_fmt);

    fmt::print("\n      ⚠  ESTES NÚMEROS NÃO MEDEM O QUE PARECEM MEDIR.\n");
    fmt::print("         Este repositório linka `libfmtd` — a variante de DEBUG\n");
    fmt::print("         da fmt, que o Conan compilou com `build_type=Debug`.\n");
    fmt::print("         Compilar ESTE arquivo com -O2 quase não muda o quadro,\n");
    fmt::print("         porque o custo está DENTRO da biblioteca, já compilada.\n");
    fmt::print("         Medido nas duas formas, com N={}:\n", N);
    fmt::print("           -O0: snprintf 18ms · format_to_n 78ms · format 91ms\n");
    fmt::print("           -O2: snprintf 18ms · format_to_n 53ms · format 57ms\n");
    fmt::print("         O `snprintf` vem da glibc, sempre otimizada.\n");

    fmt::print("\n      A ÚNICA comparação controlada aqui é entre as duas linhas\n");
    fmt::print("      da fmt, que diferem só no destino:\n");
    fmt::print("        format_to_n (buffer)  vs  format (string nova)\n");
    fmt::print("        → a diferença é o custo de ALOCAR uma std::string por\n");
    fmt::print("          iteração. Em código quente, use `format_to_n`.\n");

    fmt::print("\n      Para medir a fmt de verdade seria preciso uma build de\n");
    fmt::print("      RELEASE da biblioteca:\n");
    fmt::print("        conan install . -s build_type=Release\n");
    fmt::print("        g++ -O2 -DNDEBUG ... -lfmt      (`fmt`, não `fmtd`)\n");
    fmt::print("      Os benchmarks publicados pela fmt mostram vantagem sobre\n");
    fmt::print("      printf e iostreams nessas condições — mas isso NÃO foi\n");
    fmt::print("      reproduzido aqui, e você não deve acreditar sem medir.\n");

    fmt::print("\n      A lição metodológica vale mais que os números:\n");
    fmt::print("        1. saiba em que modo a BIBLIOTECA foi compilada, não só\n");
    fmt::print("           o seu código\n");
    fmt::print("        2. compare pares que diferem em UMA variável só\n");
    fmt::print("        3. desconfie de qualquer benchmark cujo autor não diz\n");
    fmt::print("           as flags — inclusive deste\n");

    fmt::print("\n      E o argumento que não depende de medição: `snprintf` não\n");
    fmt::print("      tem segurança de tipo, e o `ostringstream` carrega estado\n");
    fmt::print("      global pegajoso. O `format` dá as duas coisas.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_validacao();
    demo_spec();
    demo_format_to();
    demo_formatter();
    demo_custo();
}

} // namespace demo_format
