// =============================================================================
//  src/strings/demo_charconv.hpp
//
//  <charconv> — conversão texto↔número sem alocar e sem locale
//  ───────────────────────────────────────────────────────────
//  Converter "42" em 42 parece trivial, e o C++ acumulou cinco formas de
//  fazê-lo — todas com algum defeito:
//
//    atoi        — sem detecção de erro. "abc" devolve 0, igual a "0".
//    strtol      — detecta erro, mas via `errno` global e ponteiro de saída
//    stoi        — lança exceção, e o custo do throw é alto (src/errors/, 1.6)
//    stringstream— aloca, e respeita o LOCALE (o que quase nunca se quer)
//    from_chars  — sem alocação, sem locale, sem exceção. É a resposta.
//
//  `<charconv>` (C++17) existe porque parsear JSON, CSV e protocolos de rede
//  com as opções acima era caro e sutilmente errado.
//
//  Tópicos:
//  • from_chars: a API e o error_code
//  • O que ele NÃO faz (de propósito)
//  • to_chars e o round-trip garantido de ponto flutuante
//  • A armadilha do locale
//  • O custo, medido contra stoi e stringstream
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace demo_charconv {

// ─────────────────────────────────────────────────────────────────────────────
//  1. from_chars — a API
//
//     A assinatura devolve `{ptr, ec}`:
//       ptr — onde a conversão PAROU (permite continuar parseando dali)
//       ec  — errc{} em sucesso; invalid_argument ou result_out_of_range
// ─────────────────────────────────────────────────────────────────────────────
void demo_from_chars() {
    fmt::print("\n── 4.1 from_chars ──\n");

    const auto tentar = [](std::string_view s) {
        int valor = 0;
        const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), valor);

        if (ec == std::errc::invalid_argument) {
            fmt::print("      '{:<14}' → invalid_argument (nada parseável)\n", s);
        } else if (ec == std::errc::result_out_of_range) {
            fmt::print("      '{:<14}' → result_out_of_range (não cabe em int)\n", s);
        } else if (ptr != s.data() + s.size()) {
            fmt::print("      '{:<14}' → {} — mas SOBROU '{}'\n", s, valor,
                       std::string_view{ptr, static_cast<std::size_t>(s.data() + s.size() - ptr)});
        } else {
            fmt::print("      '{:<14}' → {} ✓\n", s, valor);
        }
    };

    for (std::string_view s : {"42", "-17", "3.14", "42abc", "abc",
                               "99999999999999", "", " 42"}) {
        tentar(s);
    }

    fmt::print("\n      Repare em dois casos:\n");
    fmt::print("        '3.14'  → parseou 3 e parou no '.'  (é from_chars de INT)\n");
    fmt::print("        ' 42'   → invalid_argument: ele NÃO pula espaços\n");
    fmt::print("      O `ptr` devolvido é o que permite detectar lixo sobrando —\n");
    fmt::print("      `stoi(\"42abc\")` devolve 42 sem reclamar.\n");

    // Bases diferentes, sem prefixo mágico
    int hex = 0, bin = 0;
    std::string_view h = "ff", b = "1011";
    std::from_chars(h.data(), h.data() + h.size(), hex, 16);
    std::from_chars(b.data(), b.data() + b.size(), bin, 2);
    fmt::print("\n      base explícita: 'ff' base 16 → {}, '1011' base 2 → {}\n", hex, bin);

    // Ponto flutuante, com formato explícito
    double d = 0;
    std::string_view f = "3.14159e2";
    const auto [pd, ecd] = std::from_chars(f.data(), f.data() + f.size(), d);
    fmt::print("      '3.14159e2' → {} (ec ok: {})\n", d, ecd == std::errc{});
    (void)pd;
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O que ele NÃO faz — e por que isso é bom
// ─────────────────────────────────────────────────────────────────────────────
void demo_o_que_nao_faz() {
    fmt::print("\n── 4.2 O que from_chars não faz (de propósito) ──\n");

    fmt::print("      ✗ não pula espaços em branco\n");
    fmt::print("      ✗ não aceita o prefixo '0x' (você passa a base)\n");
    fmt::print("      ✗ não aceita '+' no início (só '-')\n");
    fmt::print("      ✗ não aloca NADA\n");
    fmt::print("      ✗ não lança exceção\n");
    fmt::print("      ✗ não olha o LOCALE\n");

    fmt::print("\n      Cada um desses \"não\" é uma decisão de design:\n");
    fmt::print("      a função faz UMA coisa, do jeito mais rápido possível, e\n");
    fmt::print("      deixa as políticas para quem chama. Se você precisa pular\n");
    fmt::print("      espaços, pule antes — são duas linhas, e você paga só\n");
    fmt::print("      quando precisa.\n");

    // O padrão de uso real: parsear uma sequência inteira sem alocar
    const std::string_view csv = "10,20,30,40,50";
    std::vector<int> valores;
    valores.reserve(8);

    const char* p   = csv.data();
    const char* fim = csv.data() + csv.size();
    while (p < fim) {
        int v = 0;
        const auto [novo_p, ec] = std::from_chars(p, fim, v);
        if (ec != std::errc{}) break;
        valores.push_back(v);
        p = (novo_p < fim && *novo_p == ',') ? novo_p + 1 : fim;
    }

    fmt::print("\n      parseando \"{}\" sem uma única alocação de string:\n", csv);
    fmt::print("        → {} valores, soma = {}\n", valores.size(),
               [&] { int s = 0; for (int v : valores) s += v; return s; }());
    fmt::print("      É este padrão — `ptr` realimentando o laço — que torna\n");
    fmt::print("      `from_chars` a base de parsers de JSON e CSV rápidos.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. to_chars — e o round-trip de ponto flutuante
//
//     A garantia mais interessante: `to_chars` sem precisão produz a MENOR
//     representação que faz `from_chars` devolver exatamente o mesmo double.
//     É o algoritmo Ryū / Grisu, e nem printf("%g") nem iostreams dão isso.
// ─────────────────────────────────────────────────────────────────────────────
void demo_to_chars() {
    fmt::print("\n── 4.3 to_chars e o round-trip ──\n");

    char buf[64];

    const auto converter = [&buf](double d) {
        const auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), d);
        return ec == std::errc{} ? std::string_view{buf, static_cast<std::size_t>(ptr - buf)}
                                 : std::string_view{"<erro>"};
    };

    fmt::print("      to_chars sem precisão → a MENOR string que faz round-trip:\n");
    for (double d : {0.1, 1.0 / 3.0, 1e300, 0.3, 123456789.123456789}) {
        const auto s = converter(d);

        // Verifica o round-trip de verdade
        double volta = 0;
        std::from_chars(s.data(), s.data() + s.size(), volta);

        fmt::print("        {:<22} → '{}' {}\n",
                   fmt::format("{}", d), s, volta == d ? "✓ round-trip exato" : "✗");
    }

    fmt::print("\n      Compare com as alternativas para 0.1:\n");
    fmt::print("        printf(\"%g\")   → 0.1        (mas %g com 6 dígitos PERDE\n");
    fmt::print("                                      precisão em outros valores)\n");
    fmt::print("        printf(\"%.17g\") → sempre 17 dígitos, feio e longo\n");
    fmt::print("        to_chars        → o mínimo que preserva o valor exato\n");

    fmt::print("\n      Isso importa em: serialização JSON, formatos binários\n");
    fmt::print("      textuais, logs de telemetria — qualquer lugar em que o\n");
    fmt::print("      número precisa voltar IDÊNTICO depois de virar texto.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ A armadilha do locale
//
//     `stringstream`, `stod` e `printf` respeitam o locale global. Em pt-BR ou
//     de-DE, o separador decimal é a VÍRGULA — e um arquivo de dados escrito
//     numa máquina passa a ser ilegível na outra.
// ─────────────────────────────────────────────────────────────────────────────
void demo_locale() {
    fmt::print("\n── 4.4 ⚠ A armadilha do locale ──\n");

    fmt::print("      Com o locale global em pt_BR ou de_DE:\n");
    fmt::print("        ostringstream oss; oss << 3.14;\n");
    fmt::print("        → oss.str() == \"3,14\"    ← VÍRGULA\n");
    fmt::print("        stod(\"3.14\")  → 3.0      ← para no ponto!\n");

    fmt::print("\n      O bug que isso gera é clássico:\n");
    fmt::print("        1. o programa grava um CSV numa máquina en_US → \"3.14\"\n");
    fmt::print("        2. outra máquina, pt_BR, lê o mesmo arquivo\n");
    fmt::print("        3. `stod(\"3.14\")` devolve 3.0 e não reclama\n");
    fmt::print("        4. o dado fica errado, silenciosamente, para sempre\n");

    fmt::print("\n      E o pior: basta uma biblioteca de terceiros chamar\n");
    fmt::print("      `setlocale(LC_ALL, \"\")` na inicialização para o\n");
    fmt::print("      comportamento do SEU código mudar.\n");

    fmt::print("\n      `from_chars`/`to_chars` são IMUNES: eles não têm\n");
    fmt::print("      parâmetro de locale, por design. Sempre '.' como separador.\n");

    // Demonstração do que o próprio programa faz agora
    double d = 0;
    const std::string_view s = "3.14";
    std::from_chars(s.data(), s.data() + s.size(), d);
    fmt::print("\n      neste programa: from_chars(\"3.14\") = {} ✓\n", d);
    fmt::print("      (e continuaria {} mesmo com o locale global trocado)\n", d);

    fmt::print("\n      Regra: para DADOS (arquivos, rede, protocolos), use\n");
    fmt::print("      `<charconv>`. Locale é para APRESENTAÇÃO ao usuário —\n");
    fmt::print("      e aí `std::format` com `{{:L}}` faz isso explicitamente.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O custo, medido
// ─────────────────────────────────────────────────────────────────────────────
void demo_custo() {
    fmt::print("\n── 4.5 O custo, medido ──\n");

    constexpr int N = 200'000;
    const std::vector<std::string> entradas = [] {
        std::vector<std::string> v;
        v.reserve(1000);
        for (int i = 0; i < 1000; ++i) v.push_back(std::to_string(i * 37));
        return v;
    }();

    using clock = std::chrono::steady_clock;
    auto cronometrar = [](auto f) {
        const auto t0 = clock::now();
        const volatile long r = f();
        (void)r;
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   clock::now() - t0).count();
    };

    const auto t_from_chars = cronometrar([&] {
        long soma = 0;
        for (int i = 0; i < N; ++i) {
            const auto& s = entradas[static_cast<std::size_t>(i) % entradas.size()];
            int v = 0;
            std::from_chars(s.data(), s.data() + s.size(), v);
            soma += v;
        }
        return soma;
    });

    const auto t_stoi = cronometrar([&] {
        long soma = 0;
        for (int i = 0; i < N; ++i) {
            soma += std::stoi(entradas[static_cast<std::size_t>(i) % entradas.size()]);
        }
        return soma;
    });

    const auto t_sstream = cronometrar([&] {
        long soma = 0;
        for (int i = 0; i < N; ++i) {
            std::istringstream iss{entradas[static_cast<std::size_t>(i) % entradas.size()]};
            int v = 0;
            iss >> v;
            soma += v;
        }
        return soma;
    });

    fmt::print("      {} conversões texto → int:\n", N);
    fmt::print("        from_chars     : {:>5} ms\n", t_from_chars);
    fmt::print("        std::stoi      : {:>5} ms\n", t_stoi);
    fmt::print("        istringstream  : {:>5} ms\n", t_sstream);

    fmt::print("\n      Aqui a comparação É controlada: os três recebem a mesma\n");
    fmt::print("      `std::string` e produzem o mesmo `int`. As diferenças são\n");
    fmt::print("      estruturais e não dependem de build:\n");
    fmt::print("        • `stoi` constrói uma std::string temporária internamente\n");
    fmt::print("          e usa `strtol` + `errno`\n");
    fmt::print("        • `istringstream` constrói um OBJETO DE STREAM inteiro\n");
    fmt::print("          por conversão, com locale, sentry e flags\n");
    fmt::print("        • `from_chars` é um laço sobre os dígitos\n");

    fmt::print("\n      E há um custo que o cronômetro não vê: `stoi` LANÇA em\n");
    fmt::print("      entrada inválida. Num parser que encontra lixo com\n");
    fmt::print("      frequência, cada erro custa um unwinding completo — que\n");
    fmt::print("      em src/errors/demo_exceptions.hpp foi medido ~1000× mais\n");
    fmt::print("      caro que o caminho feliz.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_from_chars();
    demo_o_que_nao_faz();
    demo_to_chars();
    demo_locale();
    demo_custo();
}

} // namespace demo_charconv
