// =============================================================================
//  src/chrono/demo_clocks.hpp
//
//  Os relógios — e por que escolher o errado é um bug
//  ──────────────────────────────────────────────────
//  A biblioteca oferece vários relógios, e a diferença entre eles não é de
//  precisão: é de GARANTIA.
//
//    system_clock  — a hora do MUNDO. Pode andar para TRÁS (NTP, o usuário
//                    mexeu no relógio, horário de verão).
//    steady_clock  — MONOTÔNICO. Nunca anda para trás, mas não corresponde a
//                    nenhuma hora real. Conta desde um ponto arbitrário.
//    high_resolution_clock — um APELIDO para um dos dois, e qual varia por
//                    implementação. Por isso, não use.
//
//  Usar `system_clock` para medir duração é um bug clássico: se o NTP ajustar
//  o relógio no meio da medição, o resultado pode ser NEGATIVO.
//
//  Tópicos:
//  • As garantias de cada relógio
//  • ⚠ Por que `high_resolution_clock` não deve ser usado
//  • A resolução real, medida
//  • Os relógios do C++20: utc, tai, gps, file
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace demo_clocks {

namespace ch = std::chrono;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. As garantias
// ─────────────────────────────────────────────────────────────────────────────
void demo_garantias() {
    fmt::print("\n── 2.1 As garantias de cada relógio ──\n");

    fmt::print("      | relógio       | monotônico | é hora real | usar para   |\n");
    fmt::print("      |---------------|------------|-------------|-------------|\n");
    fmt::print("      | system_clock  | NÃO        | SIM         | datar       |\n");
    fmt::print("      | steady_clock  | SIM        | não         | MEDIR       |\n");
    fmt::print("      | high_resolut. | depende!   | depende!    | nada        |\n");

    fmt::print("\n      A propriedade `is_steady` diz qual é qual:\n");
    fmt::print("        system_clock::is_steady          = {}\n", ch::system_clock::is_steady);
    fmt::print("        steady_clock::is_steady          = {}\n", ch::steady_clock::is_steady);
    fmt::print("        high_resolution_clock::is_steady = {}\n",
               ch::high_resolution_clock::is_steady);

    fmt::print("\n      O bug que isso previne:\n");
    fmt::print("        auto t0 = system_clock::now();\n");
    fmt::print("        fazer_trabalho();          // ← o NTP ajusta o relógio aqui\n");
    fmt::print("        auto dt = system_clock::now() - t0;   // dt pode ser NEGATIVO\n");
    fmt::print("      Com `steady_clock`, isso é impossível por construção.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ high_resolution_clock
// ─────────────────────────────────────────────────────────────────────────────
void demo_high_resolution() {
    fmt::print("\n── 2.2 ⚠ high_resolution_clock ──\n");

    fmt::print("      O padrão permite que ele seja um APELIDO para outro relógio.\n");
    fmt::print("      Nesta implementação (libstdc++):\n");
    fmt::print("        é o mesmo que steady_clock? {}\n",
               std::is_same_v<ch::high_resolution_clock, ch::steady_clock>);
    fmt::print("        é o mesmo que system_clock? {}\n",
               std::is_same_v<ch::high_resolution_clock, ch::system_clock>);

    fmt::print("\n      ⚠  LEIA A RESPOSTA ACIMA COM ATENÇÃO.\n");
    fmt::print("         No libstdc++ (o que este programa está usando),\n");
    fmt::print("         `high_resolution_clock` É o `system_clock` — ou seja,\n");
    fmt::print("         NÃO É MONOTÔNICO:\n");
    fmt::print("           high_resolution_clock::is_steady = {}\n",
               ch::high_resolution_clock::is_steady);
    fmt::print("         Todo código que mede duração com ele está sujeito a\n");
    fmt::print("         resultado NEGATIVO se o NTP ajustar o relógio.\n");

    fmt::print("\n      E o mapeamento varia por implementação:\n");
    fmt::print("        libstdc++ (GCC)  → system_clock   ← medido acima\n");
    fmt::print("        libc++   (Clang) → steady_clock\n");
    fmt::print("        MSVC             → steady_clock\n");
    fmt::print("      O mesmo código muda de comportamento ao trocar de\n");
    fmt::print("      compilador — e o bug não aparece em teste, só em produção,\n");
    fmt::print("      quando o relógio do sistema é ajustado.\n");

    fmt::print("\n      Recomendação (a mesma do cppreference): NÃO USE.\n");
    fmt::print("      Escolha explicitamente:\n");
    fmt::print("        medir duração        → steady_clock\n");
    fmt::print("        registrar quando foi → system_clock\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. A resolução real, medida
//
//     `duration::period` diz a resolução do TIPO. A resolução EFETIVA — o menor
//     intervalo que o relógio consegue distinguir — é outra coisa, e se mede.
// ─────────────────────────────────────────────────────────────────────────────
template <typename Relogio>
void medir_resolucao(const char* nome) {
    using P = typename Relogio::period;

    // Menor diferença observável entre duas leituras consecutivas
    typename Relogio::duration menor = Relogio::duration::max();
    for (int i = 0; i < 1000; ++i) {
        const auto a = Relogio::now();
        typename Relogio::time_point b;
        do { b = Relogio::now(); } while (b == a);      // espera mudar
        const auto d = b - a;
        if (d < menor) menor = d;
    }

    fmt::print("      {:<14} tipo: {}/{} s   |  resolução medida: {} ns\n",
               nome, P::num, P::den,
               ch::duration_cast<ch::nanoseconds>(menor).count());
}

void demo_resolucao() {
    fmt::print("\n── 2.3 Resolução: do tipo vs a real ──\n");

    medir_resolucao<ch::system_clock>("system_clock");
    medir_resolucao<ch::steady_clock>("steady_clock");

    fmt::print("\n      A coluna \"tipo\" é o que o `period` PROMETE representar.\n");
    fmt::print("      A coluna \"medida\" é o menor intervalo que duas chamadas\n");
    fmt::print("      consecutivas a `now()` conseguem distinguir.\n");
    fmt::print("      Elas raramente coincidem: o tipo é nanossegundos, mas o\n");
    fmt::print("      custo de CHAMAR `now()` já é dezenas de nanossegundos.\n");

    // O custo de now(), medido
    constexpr int N = 200'000;
    const auto t0 = ch::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        const volatile auto t = ch::steady_clock::now();
        (void)t;
    }
    const auto t1 = ch::steady_clock::now();
    const auto por_chamada = ch::duration_cast<ch::nanoseconds>(t1 - t0).count() / N;

    fmt::print("\n      custo de `steady_clock::now()`: ~{} ns por chamada\n", por_chamada);
    fmt::print("      ⚠  medir algo que dura menos que isso é medir o relógio.\n");
    fmt::print("         Para trechos muito curtos, meça N repetições e divida —\n");
    fmt::print("         é o que demo_medicao.hpp mostra.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Os relógios do C++20
// ─────────────────────────────────────────────────────────────────────────────
void demo_cpp20() {
    fmt::print("\n── 2.4 Os relógios do C++20 ──\n");

    fmt::print("      | relógio      | o que é                                  |\n");
    fmt::print("      |--------------|------------------------------------------|\n");
    fmt::print("      | system_clock | Unix time — IGNORA segundos bissextos    |\n");
    fmt::print("      | utc_clock    | UTC de verdade — CONTA os bissextos      |\n");
    fmt::print("      | tai_clock    | Atômico internacional, sem bissextos     |\n");
    fmt::print("      | gps_clock    | O do sistema GPS                         |\n");
    fmt::print("      | file_clock   | O usado por std::filesystem              |\n");

    fmt::print("\n      A distinção system vs utc importa em um caso real:\n");
    fmt::print("      um segundo bissexto (leap second) faz o UTC ter 61 segundos\n");
    fmt::print("      naquele minuto. O `system_clock` finge que não aconteceu\n");
    fmt::print("      (repete um segundo); o `utc_clock` representa de verdade.\n");
    fmt::print("      Para 99% dos programas, `system_clock` é o certo.\n");

    // file_clock, que aparece ao ler timestamps de arquivo
    fmt::print("\n      `file_clock` aparece em `std::filesystem::last_write_time`.\n");
    fmt::print("      Converter para system_clock exigia um hack antes do C++20;\n");
    fmt::print("      agora há `clock_cast`:\n");
    fmt::print("        auto t = ch::clock_cast<ch::system_clock>(ft);\n");

    fmt::print("\n      ⚠  o GCC 13 tem os TIPOS, mas o suporte a fusos horários\n");
    fmt::print("         (`std::chrono::zoned_time`, o banco tzdata) depende de\n");
    fmt::print("         linkar `-ltzdata`/`libstdc++` com a feature habilitada.\n");
    fmt::print("         Ver demo_calendario.hpp, seção 4.5.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_garantias();
    demo_high_resolution();
    demo_resolucao();
    demo_cpp20();
}

} // namespace demo_clocks
