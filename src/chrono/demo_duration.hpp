// =============================================================================
//  src/chrono/demo_duration.hpp
//
//  std::chrono::duration — um sistema de unidades para tempo
//  ─────────────────────────────────────────────────────────
//  `<chrono>` não é uma biblioteca de "pegar a hora". É um SISTEMA DE UNIDADES
//  com verificação de dimensões em compilação, aplicado ao tempo — a mesma
//  ideia de src/operators/demo_literais.hpp, seção 5.2.
//
//      template <class Rep, class Period = std::ratio<1>>
//      class duration;
//
//    Rep    — o tipo que guarda a contagem (int64_t, double, ...)
//    Period — a fração de SEGUNDO que cada tique representa, como std::ratio
//
//  `milliseconds` é `duration<int64_t, ratio<1,1000>>`. O "milli" está no TIPO,
//  não numa variável — e é por isso que somar milissegundos com horas funciona
//  e somar segundos com metros não compila.
//
//  Tópicos:
//  • O que é um `duration` por dentro
//  • Conversões implícitas: quando são permitidas, e por quê
//  • duration_cast e a perda de precisão explícita
//  • Aritmética e as unidades derivadas
//  • Durations de ponto flutuante
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <chrono>
#include <cstdint>
#include <ratio>
#include <string>
#include <type_traits>

namespace demo_duration {

using namespace std::chrono_literals;
namespace ch = std::chrono;

// ─────────────────────────────────────────────────────────────────────────────
//  1. O que é um duration por dentro
// ─────────────────────────────────────────────────────────────────────────────
void demo_estrutura() {
    fmt::print("\n── 1.1 O que é um duration ──\n");

    fmt::print("      duration<Rep, Period> — Rep guarda a contagem,\n");
    fmt::print("      Period diz que fração de SEGUNDO vale cada tique.\n\n");

    const auto mostrar = []<typename D>(const char* nome) {
        using P = typename D::period;
        fmt::print("        {:<14} Rep={:<8} Period={}/{}\n", nome,
                   sizeof(typename D::rep) == 8 ? "int64_t" : "outro",
                   P::num, P::den);
    };

    mostrar.template operator()<ch::nanoseconds>("nanoseconds");
    mostrar.template operator()<ch::microseconds>("microseconds");
    mostrar.template operator()<ch::milliseconds>("milliseconds");
    mostrar.template operator()<ch::seconds>("seconds");
    mostrar.template operator()<ch::minutes>("minutes");
    mostrar.template operator()<ch::hours>("hours");
    mostrar.template operator()<ch::days>("days (C++20)");

    fmt::print("\n      sizeof(milliseconds) = {} bytes — é só o int64_t.\n",
               sizeof(ch::milliseconds));
    fmt::print("      A unidade vive no TIPO e custa ZERO em runtime.\n");

    // A prova: os tipos são distintos
    static_assert(!std::is_same_v<ch::seconds, ch::milliseconds>);
    fmt::print("      seconds e milliseconds são tipos DIFERENTES ✓\n");

    // .count() extrai a contagem crua — e perde a unidade
    const auto meio_segundo = 500ms;
    fmt::print("\n      500ms.count() = {}  ← número CRU, sem unidade\n",
               meio_segundo.count());
    fmt::print("      ⚠  `.count()` é a saída do sistema de tipos. Use só na\n");
    fmt::print("         fronteira (imprimir, gravar), nunca no meio do cálculo.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Conversões implícitas — a regra
//
//     A conversão é IMPLÍCITA quando não pode perder informação, e exige
//     `duration_cast` quando pode. É o compilador impedindo truncamento
//     silencioso.
// ─────────────────────────────────────────────────────────────────────────────
void demo_conversoes() {
    fmt::print("\n── 1.2 Conversões implícitas ──\n");

    // Unidade MAIOR → MENOR: sempre exata, logo implícita
    const ch::milliseconds de_horas = 2h;
    const ch::nanoseconds  de_ms    = 500ms;

    fmt::print("      2h   → milliseconds : {} ms   (implícito: exato)\n",
               de_horas.count());
    fmt::print("      500ms→ nanoseconds  : {} ns (implícito: exato)\n",
               de_ms.count());

    // Unidade MENOR → MAIOR: pode truncar, logo NÃO é implícita
    // const ch::seconds truncado = 1500ms;      // ✗ não compila
    fmt::print("\n      1500ms → seconds    : NÃO COMPILA\n");
    fmt::print("        (1500ms são 1,5s — converter para `seconds` truncaria)\n");
    fmt::print("      com duration_cast   : {} s  ← trunca em direção a ZERO\n",
               ch::duration_cast<ch::seconds>(1500ms).count());

    fmt::print("\n      A regra: a conversão é implícita quando o Period de\n");
    fmt::print("      destino DIVIDE exatamente o de origem.\n");
    fmt::print("        1h  → 1min  : 3600/60 = 60 exato    → implícito ✓\n");
    fmt::print("        1min→ 1h    : 60/3600 = 0,0166...   → precisa de cast\n");

    // As funções de arredondamento (C++17), quando truncar não serve
    fmt::print("\n      arredondamento explícito de 1500ms para segundos:\n");
    fmt::print("        duration_cast (trunca) : {} s\n",
               ch::duration_cast<ch::seconds>(1500ms).count());
    fmt::print("        floor                  : {} s\n",
               ch::floor<ch::seconds>(1500ms).count());
    fmt::print("        ceil                   : {} s\n",
               ch::ceil<ch::seconds>(1500ms).count());
    fmt::print("        round (p/ o par)       : {} s\n",
               ch::round<ch::seconds>(1500ms).count());

    fmt::print("\n      ⚠  para valores NEGATIVOS, `duration_cast` trunca em\n");
    fmt::print("         direção a zero e `floor` arredonda para baixo:\n");
    fmt::print("           duration_cast<seconds>(-1500ms) = {} s\n",
               ch::duration_cast<ch::seconds>(-1500ms).count());
    fmt::print("           floor<seconds>(-1500ms)         = {} s\n",
               ch::floor<ch::seconds>(-1500ms).count());
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Aritmética — e o tipo comum
//
//     Somar durations de unidades diferentes produz a unidade mais FINA que
//     representa os dois exatamente (o "common_type").
// ─────────────────────────────────────────────────────────────────────────────
void demo_aritmetica() {
    fmt::print("\n── 1.3 Aritmética ──\n");

    const auto total = 1h + 30min + 45s;
    static_assert(std::is_same_v<decltype(total), const ch::seconds>);

    fmt::print("      1h + 30min + 45s = {} s  (tipo derivado: seconds)\n",
               total.count());
    fmt::print("      ↑ o tipo comum é `seconds`: a unidade mais fina que\n");
    fmt::print("        representa hora, minuto e segundo exatamente.\n");

    const auto com_ms = 1s + 500ms;
    static_assert(std::is_same_v<decltype(com_ms), const ch::milliseconds>);
    fmt::print("      1s + 500ms = {} ms (tipo comum: milliseconds)\n", com_ms.count());

    // Multiplicação e divisão por escalar
    fmt::print("\n      2h * 3      = {} h\n", (2h * 3).count());
    fmt::print("      1h / 4      = {} min\n", ch::duration_cast<ch::minutes>(1h / 4).count());

    // Divisão de duration por duration → escalar SEM unidade
    const auto quantas_vezes = 1h / 15min;
    static_assert(std::is_integral_v<decltype(quantas_vezes)>);
    fmt::print("      1h / 15min  = {}  ← escalar puro, sem unidade\n", quantas_vezes);

    // Resto
    fmt::print("      100min % 1h = {} min\n",
               ch::duration_cast<ch::minutes>(100min % 1h).count());

    fmt::print("\n      E o que NÃO compila:\n");
    fmt::print("        1h * 1h        → tempo² não é um duration\n");
    fmt::print("        1h + 5         → 5 do quê? sem unidade, sem soma\n");
    fmt::print("      É a mesma segurança dimensional de src/operators/, 5.2 —\n");
    fmt::print("      só que aqui ela vem pronta na biblioteca padrão.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Durations de ponto flutuante
//
//     `Rep` não precisa ser inteiro. Com `double`, a conversão para unidade
//     maior deixa de perder informação — e passa a ser implícita.
// ─────────────────────────────────────────────────────────────────────────────
void demo_ponto_flutuante() {
    fmt::print("\n── 1.4 Durations de ponto flutuante ──\n");

    using SegundosF = ch::duration<double>;
    using MinutosF  = ch::duration<double, std::ratio<60>>;

    const SegundosF s = 1500ms;          // ✓ implícito! double não trunca
    const MinutosF  m = s;               // ✓ também implícito

    fmt::print("      duration<double> s = 1500ms;  → {:.3f} s (implícito ✓)\n", s.count());
    fmt::print("      duration<double,ratio<60>> m = s; → {:.4f} min\n", m.count());
    fmt::print("      ↑ com `double`, converter para unidade MAIOR não perde\n");
    fmt::print("        informação, então o compilador permite implicitamente.\n");

    // Útil para exibir medições em unidade legível
    const auto medicao = 1'234'567'890ns;
    fmt::print("\n      1234567890ns exibido em várias unidades:\n");
    fmt::print("        como double segundos : {:.6f} s\n",
               ch::duration_cast<SegundosF>(medicao).count());
    fmt::print("        como int  segundos   : {} s   ← truncou\n",
               ch::duration_cast<ch::seconds>(medicao).count());

    fmt::print("\n      ⚠  cuidado: `duration<double>` perde a EXATIDÃO que os\n");
    fmt::print("         inteiros garantem. Para contabilidade de tempo\n");
    fmt::print("         (timeouts, agendamento), prefira inteiros; para\n");
    fmt::print("         RELATÓRIO de medições, `double` é mais legível.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Unidades customizadas
// ─────────────────────────────────────────────────────────────────────────────
void demo_customizadas() {
    fmt::print("\n── 1.5 Unidades customizadas ──\n");

    // Um "frame" de vídeo a 60 fps = 1/60 de segundo
    using Frame60 = ch::duration<int64_t, std::ratio<1, 60>>;
    using Frame24 = ch::duration<int64_t, std::ratio<1, 24>>;

    const Frame60 um_segundo_de_video = 1s;
    fmt::print("      1 segundo a 60fps = {} frames\n", um_segundo_de_video.count());

    const auto duracao = Frame60{90};       // 90 frames a 60fps
    fmt::print("      90 frames a 60fps = {} ms\n",
               ch::duration_cast<ch::milliseconds>(duracao).count());

    // Converter entre taxas de quadros — com o cast explícito, porque trunca
    fmt::print("      90 frames a 60fps = {} frames a 24fps (truncado)\n",
               ch::duration_cast<Frame24>(duracao).count());

    // Uma unidade "grande": quinzenas
    using Quinzena = ch::duration<int64_t, std::ratio<14 * 24 * 3600>>;
    const Quinzena duas = Quinzena{2};
    fmt::print("      2 quinzenas = {} dias = {} horas\n",
               ch::duration_cast<ch::days>(duas).count(),
               ch::duration_cast<ch::hours>(duas).count());

    fmt::print("\n      Definir uma unidade é uma linha:\n");
    fmt::print("        using Frame60 = duration<int64_t, ratio<1, 60>>;\n");
    fmt::print("      E ela participa de toda a aritmética e de todas as\n");
    fmt::print("      conversões automaticamente — inclusive das verificações\n");
    fmt::print("      que impedem truncamento silencioso.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_estrutura();
    demo_conversoes();
    demo_aritmetica();
    demo_ponto_flutuante();
    demo_customizadas();
}

} // namespace demo_duration
