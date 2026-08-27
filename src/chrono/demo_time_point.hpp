// =============================================================================
//  src/chrono/demo_time_point.hpp
//
//  time_point — um INSTANTE, não uma duração
//  ─────────────────────────────────────────
//  A distinção que organiza tudo em `<chrono>`:
//
//    duration   — "quanto tempo"        (2 horas)
//    time_point — "quando"              (2026-08-25 14:30)
//
//  E a aritmética entre eles segue a álgebra do bom senso:
//
//    time_point - time_point = duration      ("quanto durou")
//    time_point + duration   = time_point    ("daqui a")
//    duration   + duration   = duration
//    time_point + time_point = ✗ ERRO        (não faz sentido)
//
//  Um `time_point` é internamente só uma `duration` desde a EPOCH do seu
//  relógio — e é por isso que ele carrega o tipo do relógio.
//
//  Tópicos:
//  • A estrutura: relógio + duration desde a epoch
//  • A álgebra de instantes e durações
//  • Por que time_points de relógios diferentes não se misturam
//  • Conversão para time_t e para texto
//  • time_point em APIs: deadline vs timeout
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <ratio>
#include <string>
#include <type_traits>

namespace demo_time_point {

namespace ch = std::chrono;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. A estrutura
// ─────────────────────────────────────────────────────────────────────────────
void demo_estrutura() {
    fmt::print("\n── 3.1 A estrutura de um time_point ──\n");

    const auto agora = ch::system_clock::now();

    fmt::print("      time_point<Clock, Duration> — só uma duration desde a epoch\n");
    fmt::print("      sizeof(system_clock::time_point) = {} bytes\n", sizeof(agora));

    // .time_since_epoch() é a saída para o mundo dos duration
    const auto desde_epoch = agora.time_since_epoch();
    fmt::print("\n      time_since_epoch():\n");
    fmt::print("        em segundos : {}\n",
               ch::duration_cast<ch::seconds>(desde_epoch).count());
    fmt::print("        em dias     : {}\n",
               ch::duration_cast<ch::days>(desde_epoch).count());
    fmt::print("        em anos (~) : {:.1f}\n",
               ch::duration_cast<ch::duration<double, std::ratio<31'557'600>>>(
                   desde_epoch).count());

    fmt::print("\n      A epoch de cada relógio:\n");
    fmt::print("        system_clock → 1970-01-01 00:00:00 UTC (garantido no C++20)\n");
    fmt::print("        steady_clock → um ponto ARBITRÁRIO (o boot, tipicamente)\n");
    fmt::print("      Por isso `steady_clock::now()` sozinho não significa nada —\n");
    fmt::print("      só a DIFERENÇA entre dois deles é útil.\n");

    const auto boot = ch::steady_clock::now();
    fmt::print("\n      steady_clock::now() desde a epoch dele: {} s\n",
               ch::duration_cast<ch::seconds>(boot.time_since_epoch()).count());
    fmt::print("      ↑ neste sistema, aproximadamente o uptime da máquina.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. A álgebra
// ─────────────────────────────────────────────────────────────────────────────
void demo_algebra() {
    fmt::print("\n── 3.2 A álgebra de instantes e durações ──\n");

    const auto inicio = ch::steady_clock::now();

    // Trabalho artificial, para haver o que medir
    volatile double acc = 0;
    for (int i = 0; i < 3'000'000; ++i) acc += i * 0.5;

    const auto fim = ch::steady_clock::now();

    // time_point - time_point = duration
    const auto decorrido = fim - inicio;
    static_assert(std::is_same_v<std::decay_t<decltype(decorrido)>,
                                 ch::steady_clock::duration>);

    fmt::print("      fim - inicio = {} µs   (time_point - time_point = duration)\n",
               ch::duration_cast<ch::microseconds>(decorrido).count());

    // time_point + duration = time_point
    const auto deadline = fim + 5s;
    static_assert(std::is_same_v<std::decay_t<decltype(deadline)>,
                                 ch::steady_clock::time_point>);
    fmt::print("      fim + 5s     → outro time_point (deadline)\n");
    fmt::print("      faltam {} s para o deadline\n",
               ch::duration_cast<ch::seconds>(deadline - ch::steady_clock::now()).count());

    fmt::print("\n      | expressão                | resultado  |\n");
    fmt::print("      |--------------------------|------------|\n");
    fmt::print("      | time_point - time_point  | duration   |\n");
    fmt::print("      | time_point + duration    | time_point |\n");
    fmt::print("      | time_point - duration    | time_point |\n");
    fmt::print("      | duration ± duration      | duration   |\n");
    fmt::print("      | time_point + time_point  | ✗ ERRO     |\n");

    fmt::print("\n      A última linha é a que importa: somar dois INSTANTES não\n");
    fmt::print("      tem significado, e o sistema de tipos impede. É a mesma\n");
    fmt::print("      lógica de somar duas datas de calendário.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Relógios diferentes não se misturam
// ─────────────────────────────────────────────────────────────────────────────
void demo_relogios_distintos() {
    fmt::print("\n── 3.3 Relógios diferentes não se misturam ──\n");

    const auto t_system = ch::system_clock::now();
    const auto t_steady = ch::steady_clock::now();

    // auto erro = t_system - t_steady;      // ✗ não compila
    fmt::print("      system_clock::now() - steady_clock::now() → NÃO COMPILA\n");
    fmt::print("      ↑ o relógio faz parte do TIPO do time_point, e as epochs\n");
    fmt::print("        são diferentes — a subtração não teria significado.\n");

    fmt::print("\n      são o mesmo tipo? {}\n",
               std::is_same_v<decltype(t_system), decltype(t_steady)>);

    fmt::print("\n      Para converter entre relógios (C++20): `clock_cast`\n");
    fmt::print("        auto utc = ch::clock_cast<ch::utc_clock>(t_system);\n");
    fmt::print("      ⚠  mas só entre relógios que compartilham uma referência\n");
    fmt::print("         com o mundo real. `steady_clock` NÃO participa: a\n");
    fmt::print("         epoch dele é arbitrária e não mapeia para data nenhuma.\n");

    fmt::print("\n      A consequência prática, e é uma boa:\n");
    fmt::print("        você não consegue, por acidente, medir duração com o\n");
    fmt::print("        relógio de parede e datar um evento com o monotônico.\n");
    fmt::print("        O tipo impede a troca.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Convertendo para texto e para time_t
// ─────────────────────────────────────────────────────────────────────────────
void demo_conversao_texto() {
    fmt::print("\n── 3.4 Convertendo para texto ──\n");

    const auto agora = ch::system_clock::now();

    // (a) A forma C, ainda comum em código legado
    const std::time_t t = ch::system_clock::to_time_t(agora);
    char buf[64];
    std::tm tm_local{};
    localtime_r(&t, &tm_local);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_local);
    fmt::print("      via to_time_t + strftime : {}\n", buf);
    fmt::print("        ⚠  `localtime` (sem o _r) NÃO é thread-safe: devolve um\n");
    fmt::print("           ponteiro para um buffer estático COMPARTILHADO.\n");

    // (b) A forma C++20: formatar o time_point direto
    fmt::print("\n      via fmt/std::format (C++20):\n");
    fmt::print("        {{}}          → {}\n", agora);
    fmt::print("        {{:%Y-%m-%d}} → {:%Y-%m-%d}\n", agora);
    fmt::print("        {{:%H:%M:%S}} → {:%H:%M:%S}\n", agora);

    fmt::print("\n      ⚠  o time_point de `system_clock` é UTC. Para hora local\n");
    fmt::print("         seria preciso `zoned_time` — ver demo_calendario.hpp.\n");

    // Truncar para a precisão desejada
    const auto em_segundos = ch::time_point_cast<ch::seconds>(agora);
    fmt::print("\n      truncando a precisão:\n");
    fmt::print("        original            → {}\n", agora);
    fmt::print("        time_point_cast<s>  → {}\n", em_segundos);
    fmt::print("      (útil para comparar timestamps sem os nanossegundos)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Deadline vs timeout — a distinção que evita bugs
//
//     Uma API que espera pode receber "quanto tempo esperar" (timeout) ou
//     "até quando esperar" (deadline). A segunda é mais robusta em cadeias
//     de chamadas.
// ─────────────────────────────────────────────────────────────────────────────
bool tentar_com_timeout(ch::milliseconds limite) {
    // Cada retentativa reinicia a contagem — o total pode virar N × limite
    const auto t0 = ch::steady_clock::now();
    while (ch::steady_clock::now() - t0 < limite) {
        // trabalho...
        return true;
    }
    return false;
}

bool tentar_com_deadline(ch::steady_clock::time_point limite) {
    // O deadline atravessa a cadeia: quem chama impõe o teto TOTAL
    if (ch::steady_clock::now() >= limite) return false;
    return true;
}

void demo_deadline() {
    fmt::print("\n── 3.5 Deadline vs timeout ──\n");

    fmt::print("      timeout : \"espere no MÁXIMO 100ms\"\n");
    fmt::print("      deadline: \"desista às 14:30:05.100\"\n");

    fmt::print("\n      resultado com timeout : {}\n", tentar_com_timeout(100ms));
    fmt::print("      resultado com deadline: {}\n",
               tentar_com_deadline(ch::steady_clock::now() + 100ms));

    fmt::print("\n      Por que o deadline é melhor em cadeias:\n");
    fmt::print("        conectar(timeout=100ms)\n");
    fmt::print("          → autenticar(timeout=100ms)\n");
    fmt::print("            → consultar(timeout=100ms)\n");
    fmt::print("        Total possível: 300ms, embora o chamador tenha pedido 100.\n\n");
    fmt::print("        auto limite = steady_clock::now() + 100ms;\n");
    fmt::print("        conectar(limite) → autenticar(limite) → consultar(limite)\n");
    fmt::print("        Total garantido: 100ms.\n");

    fmt::print("\n      É por isso que a biblioteca padrão oferece os DOIS:\n");
    fmt::print("        cv.wait_for(lock, 100ms)              // timeout\n");
    fmt::print("        cv.wait_until(lock, deadline)          // deadline\n");
    fmt::print("      Regra: use `_until` em qualquer coisa que possa aninhar,\n");
    fmt::print("      e `_for` só na folha da cadeia.\n");
    fmt::print("      Ver src/concurrency/demo_condition.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_estrutura();
    demo_algebra();
    demo_relogios_distintos();
    demo_conversao_texto();
    demo_deadline();
}

} // namespace demo_time_point
