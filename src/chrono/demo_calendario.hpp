// =============================================================================
//  src/chrono/demo_calendario.hpp
//
//  O calendário do C++20 — datas com segurança de tipo
//  ───────────────────────────────────────────────────
//  Até o C++17, trabalhar com datas em C++ significava `std::tm`: uma struct C
//  com campos int, em que o mês vai de 0 a 11, o ano é "anos desde 1900", e
//  nada impede `tm.tm_mon = 47`.
//
//  O C++20 trouxe um sistema de tipos completo para calendário. Cada conceito
//  vira um TIPO, e combinações inválidas são detectáveis.
//
//  Tópicos:
//  • Os tipos: year, month, day, weekday, e as combinações
//  • Construir e validar datas
//  • sys_days: a ponte entre calendário e time_point
//  • Aritmética de calendário — e por que ela é traiçoeira
//  • ⚠ Fusos horários: o que o GCC 13 tem e o que falta
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>

namespace demo_calendario {

namespace ch = std::chrono;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: os tipos de calendário do C++20 têm `operator<<` na libstdc++, mas
//  a fmt 10 (a versão deste repositório) ainda não traz `formatter` para eles —
//  isso chegou na fmt 11. O adaptador abaixo faz a ponte via ostringstream.
//
//  Com `std::format` do C++20 ou fmt 11+, `fmt::print("{}", ano)` funcionaria
//  direto e este helper seria desnecessário.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
std::string str(const T& valor) {
    std::ostringstream oss;
    oss << valor;
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. Os tipos do calendário
// ─────────────────────────────────────────────────────────────────────────────
void demo_tipos() {
    fmt::print("\n── 4.1 Os tipos do calendário ──\n");

    const ch::year    ano{2026};
    const ch::month   mes = ch::August;
    const ch::day     dia{25};
    const ch::weekday dsem = ch::Tuesday;

    fmt::print("      year{{2026}}     → {}\n", str(ano));
    fmt::print("      August         → {}\n", str(mes));
    fmt::print("      day{{25}}        → {}\n", str(dia));
    fmt::print("      Tuesday        → {}\n", str(dsem));

    fmt::print("\n      sizeof: year={} month={} day={} weekday={}\n",
               sizeof(ch::year), sizeof(ch::month), sizeof(ch::day), sizeof(ch::weekday));
    fmt::print("      ↑ tipos minúsculos; a segurança não custa memória.\n");

    // A composição com operator/ — a sintaxe mais reconhecível da biblioteca
    const auto data = ano / mes / dia;
    fmt::print("\n      composição com `/`: 2026y/August/25d → {}\n", str(data));
    fmt::print("      as três ordens funcionam, conforme a convenção local:\n");
    fmt::print("        ano/mês/dia  → {}\n", str(2026y / ch::August / 25d));
    fmt::print("        mês/dia/ano  → {}\n", str(ch::August / 25d / 2026y));
    fmt::print("        dia/mês/ano  → {}\n", str(25d / ch::August / 2026y));

    fmt::print("\n      O `operator/` foi escolhido porque já lembra data em\n");
    fmt::print("      quase toda notação do mundo — e porque não colidia com\n");
    fmt::print("      nenhum uso existente nesses tipos.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Validação — o que `std::tm` nunca teve
// ─────────────────────────────────────────────────────────────────────────────
void demo_validacao() {
    fmt::print("\n── 4.2 Validação ──\n");

    const auto validas = {2026y / ch::February / 28d, 2024y / ch::February / 29d};
    const auto invalidas = {2026y / ch::February / 30d, 2026y / ch::February / 29d,
                            2026y / ch::April / 31d};

    fmt::print("      datas válidas:\n");
    for (const auto& d : validas) {
        fmt::print("        {} → ok() = {}\n", str(d), d.ok());
    }

    fmt::print("      datas inválidas:\n");
    for (const auto& d : invalidas) {
        fmt::print("        {} → ok() = {}\n", str(d), d.ok());
    }

    fmt::print("\n      ⚠  `ok()` não é chamado automaticamente. Construir uma\n");
    fmt::print("         data inválida COMPILA e não lança — você é responsável\n");
    fmt::print("         por verificar na fronteira de entrada.\n");

    // Ano bissexto vem de graça
    fmt::print("\n      anos bissextos (a regra completa, não só 'divisível por 4'):\n");
    for (int a : {1900, 2000, 2024, 2026, 2100}) {
        fmt::print("        {} → is_leap() = {}\n", a, ch::year{a}.is_leap());
    }
    fmt::print("      ↑ 1900 e 2100 NÃO são bissextos (divisíveis por 100 mas\n");
    fmt::print("        não por 400). É o bug que quebrou sistemas em 1900.\n");

    // Último dia do mês, sem tabela na mão
    fmt::print("\n      último dia de cada fevereiro:\n");
    for (int a : {2024, 2025, 2026}) {
        const auto ultimo = ch::year{a} / ch::February / ch::last;
        // `year_month_day_last` imprime como "2024/Feb/last"; converter para
        // `year_month_day` RESOLVE qual é o dia — 28 ou 29 conforme o ano.
        fmt::print("        {} → {} (resolvido: {})\n", a, str(ultimo),
                   str(ch::year_month_day{ultimo}));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. sys_days — a ponte entre calendário e time_point
//
//     `sys_days` é um `time_point<system_clock, days>`. Converter uma data
//     para ele dá acesso a toda a aritmética de duração.
// ─────────────────────────────────────────────────────────────────────────────
void demo_sys_days() {
    fmt::print("\n── 4.3 sys_days — a ponte ──\n");

    const auto data  = 2026y / ch::August / 25d;
    const auto ponto = ch::sys_days{data};

    fmt::print("      2026-08-25 como sys_days → {} dias desde a epoch\n",
               ponto.time_since_epoch().count());

    // Que dia da semana é? Basta converter.
    const ch::weekday dsem{ponto};
    fmt::print("      dia da semana → {}\n", str(dsem));

    // Diferença entre datas = duration em dias
    const auto natal = ch::sys_days{2026y / ch::December / 25d};
    const auto faltam = natal - ponto;
    fmt::print("      até 2026-12-25 → {} dias\n", faltam.count());

    // E o caminho de volta
    const auto daqui_a_100 = ponto + ch::days{100};
    fmt::print("      +100 dias      → {}\n", str(ch::year_month_day{daqui_a_100}));

    // Combinar com hora do dia
    const auto instante = ponto + 14h + 30min;
    fmt::print("      com hora       → {:%Y-%m-%d %H:%M}\n", instante);

    fmt::print("\n      O padrão de uso:\n");
    fmt::print("        year_month_day  ← para CALENDÁRIO (mês, ano, validação)\n");
    fmt::print("        sys_days        ← para ARITMÉTICA (somar dias, diferença)\n");
    fmt::print("      Converta entre os dois conforme a operação. As conversões\n");
    fmt::print("      são baratas: `sys_days` é um int64 de dias.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ Aritmética de calendário é traiçoeira
//
//     Somar meses e somar dias são operações DIFERENTES, e o resultado de
//     somar meses pode ser uma data inválida.
// ─────────────────────────────────────────────────────────────────────────────
void demo_aritmetica() {
    fmt::print("\n── 4.4 ⚠ Aritmética de calendário ──\n");

    const auto jan31 = 2026y / ch::January / 31d;

    // Somar um MÊS: o dia é preservado — e 31 de fevereiro não existe
    const auto mais_um_mes = jan31 + ch::months{1};
    fmt::print("      2026-01-31 + 1 mês  = {}  → ok() = {}\n",
               str(mais_um_mes), mais_um_mes.ok());
    fmt::print("      ↑ a biblioteca NÃO \"conserta\" para 28/02. Ela devolve\n");
    fmt::print("        uma data inválida e deixa VOCÊ decidir a política.\n");

    fmt::print("\n      as três políticas possíveis, e como aplicar cada uma:\n");
    fmt::print("        último dia do mês : {}\n",
               str(ch::year_month_day{ch::sys_days{
                   mais_um_mes.year() / mais_um_mes.month() / ch::last}}));
    fmt::print("        transbordar       : {}\n",
               str(ch::year_month_day{ch::sys_days{mais_um_mes}}));
    fmt::print("        rejeitar          : lançar/erro, porque ok() == false\n");
    fmt::print("      ↑ note que `sys_days{{data_inválida}}` NORMALIZA (31/02 vira\n");
    fmt::print("        03/03), que é a política \"transbordar\".\n");

    // Somar DIAS nunca produz data inválida
    const auto mais_31_dias = ch::year_month_day{ch::sys_days{jan31} + ch::days{31}};
    fmt::print("\n      2026-01-31 + 31 dias = {}  → ok() = {}\n",
               str(mais_31_dias), mais_31_dias.ok());
    fmt::print("      ↑ aritmética de DIAS é sempre válida; de MESES, não.\n");

    fmt::print("\n      | operação        | tipo              | pode ser inválida |\n");
    fmt::print("      |-----------------|-------------------|-------------------|\n");
    fmt::print("      | + days          | sys_days          | não               |\n");
    fmt::print("      | + months        | year_month_day    | SIM               |\n");
    fmt::print("      | + years         | year_month_day    | SIM (29/02)       |\n");

    // O caso do 29 de fevereiro
    const auto bissexto = 2024y / ch::February / 29d;
    const auto mais_um_ano = bissexto + ch::years{1};
    fmt::print("\n      2024-02-29 + 1 ano  = {}  → ok() = {}\n",
               str(mais_um_ano), mais_um_ano.ok());
    fmt::print("      ↑ o mesmo problema, com aniversários de quem nasceu em 29/02.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. ⚠ Fusos horários — o que o GCC 13 tem
// ─────────────────────────────────────────────────────────────────────────────
void demo_fusos() {
    fmt::print("\n── 4.5 ⚠ Fusos horários ──\n");

    fmt::print("      A API do C++20 para fusos é:\n");
    fmt::print("        auto tz  = ch::current_zone();\n");
    fmt::print("        auto zt  = ch::zoned_time{{tz, ch::system_clock::now()}};\n");
    fmt::print("        fmt::print(\"{{}}\", zt);      // hora LOCAL, com o fuso\n");

    fmt::print("\n      Ela depende do banco de dados de fusos (tzdata) e do\n");
    fmt::print("      suporte compilado na libstdc++.\n");

    // Detecção em tempo de compilação do suporte
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    fmt::print("\n      __cpp_lib_chrono = {} (>= 201907 = API de fusos presente)\n",
               __cpp_lib_chrono);
#else
    fmt::print("\n      __cpp_lib_chrono não indica suporte a fusos.\n");
#endif

    fmt::print("\n      ⚠  neste ambiente, tentar usar `current_zone()` pode\n");
    fmt::print("         lançar `std::runtime_error` se o tzdata não estiver\n");
    fmt::print("         instalado, ou falhar no LINK em libstdc++ mais antigas.\n");
    fmt::print("         O demo não chama a função de propósito — verifique com:\n");
    fmt::print("           echo '#include <chrono>\\nint main(){{'\n");
    fmt::print("           '  auto z = std::chrono::current_zone(); }}' | g++ -std=c++23 -x c++ -\n");

    fmt::print("\n      Enquanto isso, a conversão para hora local segue possível\n");
    fmt::print("      pela via C, que é o que demo_time_point.hpp, 3.4 mostra:\n");
    fmt::print("        localtime_r + strftime\n");

    fmt::print("\n      A regra que não muda, independente de suporte:\n");
    fmt::print("        ARMAZENE sempre em UTC (system_clock).\n");
    fmt::print("        CONVERTA para local só na APRESENTAÇÃO.\n");
    fmt::print("      Guardar hora local em banco de dados é a origem de metade\n");
    fmt::print("      dos bugs de horário de verão que existem.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_tipos();
    demo_validacao();
    demo_sys_days();
    demo_aritmetica();
    demo_fusos();
}

} // namespace demo_calendario
