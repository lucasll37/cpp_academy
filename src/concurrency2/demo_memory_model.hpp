// =============================================================================
//  src/concurrency2/demo_memory_model.hpp
//
//  O modelo de memória — o que "acontece antes" significa
//  ──────────────────────────────────────────────────────
//  Esta é a parte da linguagem em que a intuição mais falha. A causa é
//  simples de enunciar e difícil de aceitar:
//
//      NÃO EXISTE "a ordem em que o programa executou".
//
//  O compilador reordena. A CPU reordena. Os caches propagam escritas em
//  ordens diferentes para núcleos diferentes. O que o C++11 definiu não foi
//  uma ordem global, e sim uma relação PARCIAL — `happens-before` — e a regra
//  de que ler algo que não acontece-antes da sua leitura é data race, ou seja,
//  UB total (ver src/undefined_behavior/).
//
//  Tópicos:
//  • Data race é UB — não é "valor errado"
//  • seq_cst: o padrão, e por que é o padrão
//  • acquire/release: o par que publica dados
//  • relaxed: só atomicidade, nenhuma ordem
//  • ⚠ o que memory_order NÃO faz
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

namespace demo_memory_model {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. Data race é UB
// ─────────────────────────────────────────────────────────────────────────────
void demo_data_race() {
    fmt::print("\n── 3.1 Data race é UB, não \"valor errado\" ──\n");

    fmt::print("      Duas threads, uma escreve e outra lê o mesmo `int` sem\n");
    fmt::print("      sincronização = data race = comportamento indefinido.\n\n");

    fmt::print("      A intuição errada: \"no pior caso leio um valor velho\".\n");
    fmt::print("      A realidade: o compilador pode assumir que o race NÃO\n");
    fmt::print("      acontece e otimizar em cima disso. O clássico:\n\n");
    fmt::print("        bool parar = false;              // NÃO atômico\n");
    fmt::print("        while (!parar) {{ trabalho(); }}    // outra thread põe true\n\n");
    fmt::print("      Com -O2, o compilador vê que o laço não escreve em `parar`,\n");
    fmt::print("      iça a leitura para fora e gera `while(true)`. O programa\n");
    fmt::print("      trava para sempre — e o compilador está CERTO, porque a\n");
    fmt::print("      outra thread escrevendo ali já era UB.\n\n");

    // A versão correta, medida
    std::atomic<bool> parar{false};
    std::atomic<long> voltas{0};

    std::jthread t{[&] {
        while (!parar.load()) {          // atomic → o compilador precisa reler
            voltas.fetch_add(1, std::memory_order_relaxed);
        }
    }};
    std::this_thread::sleep_for(20ms);
    parar = true;
    t.join();

    fmt::print("      Com `std::atomic<bool>`: o laço deu {} voltas e terminou.\n",
               voltas.load());
    fmt::print("      `volatile` NÃO resolve: ele impede a otimização da leitura,\n");
    fmt::print("      mas não dá nenhuma garantia de ordem entre variáveis nem\n");
    fmt::print("      atomicidade. Em C++, `volatile` é para MMIO, não para threads.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. acquire / release — o par que publica dados
//
//     A regra em uma frase:
//       se a thread B faz um LOAD-ACQUIRE que LÊ o valor escrito por um
//       STORE-RELEASE da thread A, então tudo que A escreveu ANTES do store
//       é visível para B DEPOIS do load.
//
//     É assim que se publica uma estrutura inteira com um único atômico.
// ─────────────────────────────────────────────────────────────────────────────
void demo_acquire_release() {
    fmt::print("\n── 3.2 acquire / release ──\n");

    struct Dados {
        int          a = 0;
        double       b = 0.0;
        std::string  c;
    };

    Dados             carga;             // NÃO atômico — de propósito
    std::atomic<bool> pronto{false};     // a "bandeira"

    std::atomic<bool> ok{false};

    std::jthread leitor{[&] {
        while (!pronto.load(std::memory_order_acquire)) {   // ← ACQUIRE
            std::this_thread::yield();
        }
        // Aqui é SEGURO ler `carga` sem ser atômica: o par release/acquire
        // criou happens-before entre as escritas do produtor e estas leituras.
        ok = (carga.a == 42 && carga.b == 3.5 && carga.c == "publicado");
    }};

    carga.a = 42;                                        // ┐
    carga.b = 3.5;                                       // │ escritas comuns
    carga.c = "publicado";                               // ┘
    pronto.store(true, std::memory_order_release);       // ← RELEASE publica tudo

    leitor.join();
    fmt::print("      leitor viu a struct completa e consistente? {}\n", ok.load());

    fmt::print("\n      A barreira é DIRECIONAL, e é o que a torna barata:\n");
    fmt::print("        release → nada de ANTES pode descer para depois do store\n");
    fmt::print("        acquire → nada de DEPOIS pode subir para antes do load\n");
    fmt::print("      Cada um bloqueia UM sentido. `seq_cst` bloqueia os dois e\n");
    fmt::print("      ainda impõe uma ordem total entre TODAS as operações seq_cst.\n\n");

    fmt::print("      ⚠  release sozinho não faz nada. Só existe sincronização se\n");
    fmt::print("         o acquire LER o valor que aquele release escreveu. Ler um\n");
    fmt::print("         valor mais antigo não sincroniza com nada.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. relaxed — só atomicidade
// ─────────────────────────────────────────────────────────────────────────────
void demo_relaxed() {
    fmt::print("\n── 3.3 memory_order_relaxed ──\n");

    // Um contador é o caso legítimo clássico: ninguém depende da ORDEM dos
    // incrementos, só de que nenhum se perca.
    std::atomic<long> contador{0};
    constexpr int kThreads = 4, kPorThread = 50'000;

    {
        std::vector<std::jthread> ts;
        for (int i = 0; i < kThreads; ++i) {
            ts.emplace_back([&contador] {
                for (int k = 0; k < kPorThread; ++k)
                    contador.fetch_add(1, std::memory_order_relaxed);
            });
        }
    }

    const long esperado = static_cast<long>(kThreads) * kPorThread;
    fmt::print("      contador relaxed: {} (esperado {}) — {}\n",
               contador.load(), esperado,
               contador.load() == esperado ? "nenhum incremento perdido"
                                           : "ERRO");

    fmt::print("\n      `relaxed` garante APENAS que a operação é atômica: nenhum\n");
    fmt::print("      incremento se perde, nenhum valor \"meio escrito\" aparece.\n");
    fmt::print("      Não garante NADA sobre a ordem em relação a outras variáveis.\n\n");

    fmt::print("      Legítimo:  contadores de estatística, flags de \"já vi isso\",\n");
    fmt::print("                 incremento de refcount de shared_ptr.\n");
    fmt::print("      Errado:    publicar dados. Isto NÃO funciona:\n");
    fmt::print("        dados = 42;\n");
    fmt::print("        pronto.store(true, memory_order_relaxed);   // ← sem ordem!\n");
    fmt::print("      A outra thread pode ver `pronto == true` e `dados == 0`.\n\n");

    fmt::print("      Detalhe do refcount: o INCREMENTO pode ser relaxed, mas o\n");
    fmt::print("      DECREMENTO precisa ser acq_rel — quem zera o contador vai\n");
    fmt::print("      destruir o objeto, e precisa enxergar tudo que as outras\n");
    fmt::print("      threads fizeram com ele. Ver src/smart-pointers/.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. seq_cst — o padrão, e o preço
// ─────────────────────────────────────────────────────────────────────────────
void demo_seq_cst() {
    fmt::print("\n── 3.4 seq_cst: o padrão ──\n");

    fmt::print("      `x.store(1)` é `x.store(1, memory_order_seq_cst)`.\n");
    fmt::print("      Ele dá a única garantia que casa com a intuição: existe UMA\n");
    fmt::print("      ordem total de todas as operações seq_cst, e todas as threads\n");
    fmt::print("      concordam com ela.\n\n");

    // Medição do custo relativo
    constexpr int kIters = 2'000'000;
    auto medir = [](std::memory_order ordem) {
        std::atomic<long> x{0};
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kIters; ++i) x.fetch_add(1, ordem);
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    };

    const double t_relaxed = medir(std::memory_order_relaxed);
    const double t_seqcst  = medir(std::memory_order_seq_cst);

    fmt::print("      {} fetch_add SEM contenção (uma thread só):\n", kIters);
    fmt::print("        relaxed : {:7.2f} ms\n", t_relaxed);
    fmt::print("        seq_cst : {:7.2f} ms\n", t_seqcst);
    fmt::print("        razão   : {:7.2f}×\n", t_seqcst / t_relaxed);

    fmt::print("\n      No x86-64 a diferença aqui costuma ser pequena, porque\n");
    fmt::print("      `fetch_add` já vira `lock xadd` de qualquer forma — a\n");
    fmt::print("      arquitetura é fortemente ordenada e acquire/release saem de\n");
    fmt::print("      graça. Num ARM ou POWER, onde `seq_cst` precisa de instruções\n");
    fmt::print("      de barreira explícitas (`dmb ish`), a diferença é grande.\n");
    fmt::print("      É por isso que \"otimizei trocando para relaxed\" precisa ser\n");
    fmt::print("      MEDIDO na arquitetura alvo, e não presumido.\n\n");

    fmt::print("      Regra prática: comece com seq_cst (o padrão). Só desça para\n");
    fmt::print("      acquire/release ou relaxed quando (a) o profiler apontar,\n");
    fmt::print("      e (b) você conseguir escrever qual happens-before está\n");
    fmt::print("      preservando. Se não consegue escrever, não desça.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. ⚠ o que memory_order NÃO faz
// ─────────────────────────────────────────────────────────────────────────────
void demo_armadilhas() {
    fmt::print("\n── 3.5 ⚠ Armadilhas ──\n");

    fmt::print("      (a) atômico NÃO é thread-safe automaticamente.\n");
    fmt::print("          std::atomic<int> x;\n");
    fmt::print("          if (x > 0) x = x - 1;    // LEU e ESCREVEU: duas operações\n");
    fmt::print("          Entre o if e o store outra thread mexe. Precisa de\n");
    fmt::print("          `compare_exchange` num laço, ou de um mutex.\n\n");

    fmt::print("      (b) `atomic<T>` com T grande usa LOCK escondido.\n");
    {
        struct Grande { double a, b, c, d; };

        // `is_always_lock_free` é um `static constexpr bool`: responde em
        // COMPILAÇÃO. O `is_lock_free()` de membro responde em runtime — e,
        // para tipos grandes, vira uma chamada a `__atomic_is_lock_free` da
        // libatomic, obrigando a linkar `-latomic`. Prefira o constexpr.
        static_assert(std::atomic<int>::is_always_lock_free);

        fmt::print("          atomic<int>     ({:2} bytes)  lock-free? {}\n",
                   sizeof(int), std::atomic<int>::is_always_lock_free);
        fmt::print("          atomic<double>  ({:2} bytes)  lock-free? {}\n",
                   sizeof(double), std::atomic<double>::is_always_lock_free);
        fmt::print("          atomic<Grande>  ({:2} bytes)  lock-free? {}\n",
                   sizeof(Grande), std::atomic<Grande>::is_always_lock_free);
        fmt::print("          O corte no x86-64 é 16 bytes (`cmpxchg16b`).\n");
        fmt::print("          Quando é false, a implementação usa um mutex interno.\n");
        fmt::print("          Continua correto — mas não é lock-free, não pode ser\n");
        fmt::print("          usado em signal handler, e o `is_lock_free()` de\n");
        fmt::print("          runtime exige linkar `-latomic`.\n\n");
    }

    fmt::print("      (c) `memory_order_consume` é para não usar. Nenhum compilador\n");
    fmt::print("          o implementa de verdade: todos o promovem para acquire.\n");
    fmt::print("          Está em revisão desde C++17.\n\n");

    fmt::print("      (d) O par tem que ser NO MESMO átomo. Fazer release em `a` e\n");
    fmt::print("          acquire em `b` não sincroniza nada.\n\n");

    fmt::print("      (e) Nada disso substitui testar com TSan:\n");
    fmt::print("            g++ -fsanitize=thread ...\n");
    fmt::print("          O TSan constrói o grafo de happens-before em runtime e\n");
    fmt::print("          acusa o race mesmo quando ele não se manifesta.\n");
    fmt::print("          Ver src/undefined_behavior/demo_sanitizers.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_data_race();
    demo_acquire_release();
    demo_relaxed();
    demo_seq_cst();
    demo_armadilhas();
}

}  // namespace demo_memory_model
