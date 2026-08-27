// =============================================================================
//  src/concurrency2/demo_sincronizadores.hpp
//
//  latch, barrier e counting_semaphore — os primitivos do C++20
//  ────────────────────────────────────────────────────────────
//  Antes do C++20, coordenar N threads exigia `mutex` + `condition_variable`
//  + um contador — três objetos e umas quinze linhas para expressar
//  "espere todo mundo chegar". O C++20 trouxe três primitivos que dizem
//  exatamente isso, e que o runtime pode implementar com futex direto.
//
//    latch      → porteira de USO ÚNICO      ("largada da corrida")
//    barrier    → porteira REUTILIZÁVEL      ("fim de cada rodada")
//    semaphore  → contador de PERMISSÕES     ("só 3 entram de cada vez")
//
//  Tópicos:
//  • latch: count_down / wait / arrive_and_wait
//  • barrier: fases e a função de completion
//  • counting_semaphore e binary_semaphore
//  • ⚠ semaphore ≠ mutex (não tem dono)
//  • O que cada um substitui do código antigo
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <atomic>
#include <barrier>
#include <chrono>
#include <latch>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

namespace demo_sincronizadores {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::latch — a porteira de uso único
//
//     Um contador que só desce. Quem espera fica bloqueado até ele chegar a 0.
//     Chegou a 0, fica em 0 para sempre: não há reset. Daí "uso único".
// ─────────────────────────────────────────────────────────────────────────────
void demo_latch() {
    fmt::print("\n── 2.1 std::latch ──\n");

    constexpr int kThreads = 4;

    // Uso A: a thread principal espera todas as trabalhadoras terminarem.
    std::latch terminaram{kThreads};
    std::vector<std::jthread> ts;

    for (int i = 0; i < kThreads; ++i) {
        ts.emplace_back([i, &terminaram] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5 * (i + 1)));
            fmt::print("      worker {} terminou\n", i);
            terminaram.count_down();      // -1, e acorda quem espera se chegou a 0
        });
    }

    terminaram.wait();                    // bloqueia até o contador zerar
    fmt::print("      ✓ main: todos os {} terminaram\n", kThreads);
    ts.clear();

    // Uso B: a LARGADA — todas partem no mesmo instante.
    // Sem isso, a thread 0 já processou metade quando a thread 3 nasce, e
    // qualquer medição de paralelismo fica sem sentido.
    fmt::print("\n      largada sincronizada:\n");
    std::latch largada{1};                // 1 só: o tiro de partida
    std::latch prontos{kThreads};
    std::atomic<int> ordem{0};

    std::vector<std::jthread> corredores;
    for (int i = 0; i < kThreads; ++i) {
        corredores.emplace_back([i, &largada, &prontos, &ordem] {
            prontos.count_down();         // "estou na posição"
            largada.wait();               // espera o tiro
            fmt::print("      corredor {} partiu (#{} a sair)\n",
                       i, ordem.fetch_add(1) + 1);
        });
    }

    prontos.wait();                       // todos posicionados
    fmt::print("      → TIRO\n");
    largada.count_down();                 // libera os 4 de uma vez
    corredores.clear();                   // ~jthread faz join

    fmt::print("\n      `arrive_and_wait()` = count_down() + wait() num só passo,\n");
    fmt::print("      para quem participa E espera.\n");
    fmt::print("      ⚠  count_down() além do zero é UB. Não há como \"desfazer\".\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. std::barrier — a porteira reutilizável
//
//     Igual ao latch, mas ao chegar a zero ele SE REARMA para a fase seguinte.
//     É o primitivo de simulações em passos: ninguém começa o passo N+1 antes
//     de todos terminarem o passo N.
// ─────────────────────────────────────────────────────────────────────────────
void demo_barrier() {
    fmt::print("\n── 2.2 std::barrier ──\n");

    constexpr int kThreads = 3;
    constexpr int kRodadas = 3;

    std::atomic<int> fase{0};

    // O segundo argumento é a "completion function": roda UMA vez, em UMA das
    // threads participantes, no instante em que a última chega — e antes de
    // qualquer uma ser liberada. É o lugar seguro para trocar buffers.
    std::barrier sincro{kThreads, [&fase]() noexcept {
        fmt::print("      ── fim da fase {} (completion) ──\n", fase.fetch_add(1));
    }};

    std::vector<std::jthread> ts;
    for (int i = 0; i < kThreads; ++i) {
        ts.emplace_back([i, &sincro] {
            for (int r = 0; r < kRodadas; ++r) {
                std::this_thread::sleep_for(std::chrono::milliseconds(3 * (i + 1)));
                fmt::print("      thread {} fez a rodada {}\n", i, r);
                sincro.arrive_and_wait();   // espera as outras duas
            }
        });
    }
    ts.clear();

    fmt::print("      ✓ {} rodadas × {} threads, sempre em sincronia\n",
               kRodadas, kThreads);

    fmt::print("\n      Note a ordem da saída: dentro de uma fase as threads se\n");
    fmt::print("      intercalam livremente, mas NENHUMA linha da fase N+1 aparece\n");
    fmt::print("      antes do 'completion' da fase N. Essa é a garantia.\n");

    fmt::print("\n      `arrive_and_drop()` reduz o número esperado nas próximas\n");
    fmt::print("      fases — para uma thread que vai sair mais cedo. Sem isso,\n");
    fmt::print("      as restantes esperariam para sempre por quem já morreu.\n");

    fmt::print("\n      | primitivo | reutilizável | completion |\n");
    fmt::print("      |-----------|--------------|------------|\n");
    fmt::print("      | latch     | não          | não        |\n");
    fmt::print("      | barrier   | sim          | sim        |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. counting_semaphore — permissões
// ─────────────────────────────────────────────────────────────────────────────
void demo_semaphore() {
    fmt::print("\n── 2.3 counting_semaphore ──\n");

    // O parâmetro de template é o MÁXIMO possível; o do construtor é o valor
    // inicial. Aqui: no máximo 8, começando com 2 permissões livres.
    std::counting_semaphore<8> vagas{2};

    std::atomic<int> dentro{0};
    std::atomic<int> pico{0};

    std::vector<std::jthread> ts;
    for (int i = 0; i < 6; ++i) {
        ts.emplace_back([i, &vagas, &dentro, &pico] {
            vagas.acquire();                     // -1 (bloqueia se estiver em 0)

            int agora = dentro.fetch_add(1) + 1;
            // atualiza o pico observado
            int p = pico.load();
            while (agora > p && !pico.compare_exchange_weak(p, agora)) { }

            fmt::print("      tarefa {} entrou (dentro agora: {})\n", i, agora);
            std::this_thread::sleep_for(10ms);
            dentro.fetch_sub(1);

            vagas.release();                     // +1, acorda um esperador
        });
    }
    ts.clear();

    fmt::print("      ✓ 6 tarefas, no máximo {} simultâneas (medido)\n", pico.load());

    // binary_semaphore = counting_semaphore<1>, um alias
    fmt::print("\n      binary_semaphore (= counting_semaphore<1>) como sinal:\n");
    std::binary_semaphore pronto{0};             // começa SEM permissão

    std::jthread produtor{[&pronto] {
        std::this_thread::sleep_for(15ms);
        fmt::print("      [produtor] dado pronto, sinalizando\n");
        pronto.release();
    }};
    pronto.acquire();                            // bloqueia até o release
    fmt::print("      [main] recebi o sinal\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ semaphore NÃO é mutex
// ─────────────────────────────────────────────────────────────────────────────
void demo_semaphore_vs_mutex() {
    fmt::print("\n── 2.4 ⚠ semaphore não é mutex ──\n");

    fmt::print("      | aspecto        | mutex               | semaphore          |\n");
    fmt::print("      |----------------|---------------------|--------------------|\n");
    fmt::print("      | dono           | SIM, quem travou    | NÃO, ninguém       |\n");
    fmt::print("      | quem libera    | só o dono           | qualquer thread    |\n");
    fmt::print("      | contagem       | 0 ou 1              | 0..N               |\n");
    fmt::print("      | RAII           | scoped_lock         | não há padrão      |\n");
    fmt::print("      | uso típico     | exclusão mútua      | recursos limitados |\n\n");

    fmt::print("      Um `counting_semaphore<1>` PARECE um mutex, mas:\n");
    fmt::print("        • a thread A pode dar `acquire` e a thread B dar `release`\n");
    fmt::print("          — isso é RECURSO em semáforo, e BUG em mutex;\n");
    fmt::print("        • não há `unique_lock` para ele: um `return` no meio ou uma\n");
    fmt::print("          exceção vazam a permissão para sempre.\n\n");

    fmt::print("      Se você quer exclusão mútua, use `mutex` + `scoped_lock`\n");
    fmt::print("      (src/concurrency/demo_mutex.hpp). Semáforo é para CONTAR:\n");
    fmt::print("      \"no máximo 4 conexões\", \"o buffer tem 10 vagas\".\n\n");

    fmt::print("      Um RAII caseiro, se realmente precisar:\n");
    fmt::print("        struct Permissao {{\n");
    fmt::print("            std::counting_semaphore<8>& s;\n");
    fmt::print("            explicit Permissao(auto& sem) : s{{sem}} {{ s.acquire(); }}\n");
    fmt::print("            ~Permissao() {{ s.release(); }}\n");
    fmt::print("            Permissao(const Permissao&) = delete;   // Rule of 5\n");
    fmt::print("        }};\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O que cada um substitui
// ─────────────────────────────────────────────────────────────────────────────
void demo_substituicoes() {
    fmt::print("\n── 2.5 O código que estes primitivos aposentam ──\n");

    fmt::print("      ANTES (esperar N threads):\n");
    fmt::print("        std::mutex m; std::condition_variable cv; int restam = N;\n");
    fmt::print("        // na worker:\n");
    fmt::print("        {{ std::scoped_lock lk{{m}}; if (--restam == 0) cv.notify_all(); }}\n");
    fmt::print("        // na main:\n");
    fmt::print("        std::unique_lock lk{{m}}; cv.wait(lk, [&]{{ return restam == 0; }});\n\n");
    fmt::print("      DEPOIS:\n");
    fmt::print("        std::latch l{{N}};\n");
    fmt::print("        l.count_down();   // worker\n");
    fmt::print("        l.wait();         // main\n\n");

    fmt::print("      Não é só menos linhas. A versão antiga:\n");
    fmt::print("        • aloca e trava um mutex a cada decremento;\n");
    fmt::print("        • acorda TODAS as threads no notify_all;\n");
    fmt::print("        • deixa espaço para esquecer o predicado e sofrer\n");
    fmt::print("          spurious wakeup (ver src/concurrency/demo_condition.hpp).\n");
    fmt::print("      A do latch pode ser um `futex` e um contador atômico.\n\n");

    fmt::print("      Escolha rápida:\n");
    fmt::print("        esperar N eventos, uma vez ............... latch\n");
    fmt::print("        sincronizar rodadas repetidas ............ barrier\n");
    fmt::print("        limitar acesso concorrente a K ........... counting_semaphore\n");
    fmt::print("        sinalizar \"aconteceu\" uma vez ............ binary_semaphore\n");
    fmt::print("        exclusão mútua ........................... mutex\n");
    fmt::print("        resultado de UMA operação assíncrona ..... future/promise\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_latch();
    demo_barrier();
    demo_semaphore();
    demo_semaphore_vs_mutex();
    demo_substituicoes();
}

}  // namespace demo_sincronizadores
