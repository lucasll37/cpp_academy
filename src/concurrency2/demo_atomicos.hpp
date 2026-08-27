// =============================================================================
//  src/concurrency2/demo_atomicos.hpp
//
//  CAS, atomic_flag e a espera atômica do C++20
//  ────────────────────────────────────────────
//  `fetch_add` resolve contadores. Tudo além disso — "incremente só se ainda
//  for X", "insira na frente da lista", "atualize o máximo" — precisa da
//  única primitiva que realmente importa em programação sem locks:
//
//      compare_exchange:  "se o valor ainda for `esperado`, troque por
//                          `novo`; senão, me diga o que ele é agora"
//
//  Tudo o mais (fetch_add, fetch_or, exchange) pode ser construído a partir
//  dela. O hardware a expõe como `lock cmpxchg` no x86 e como o par
//  `ldrex/strex` (LL/SC) no ARM.
//
//  Tópicos:
//  • compare_exchange_weak vs _strong
//  • O laço de CAS como padrão
//  • atomic_flag — o único sempre-lock-free
//  • atomic::wait / notify (C++20) — bloquear sem mutex
//  • ⚠ spin lock: por que quase sempre é a escolha errada
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace demo_atomicos {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. O laço de CAS
//
//     A forma canônica. Repare que `esperado` é passado por REFERÊNCIA e é
//     ATUALIZADO pela função quando ela falha — é isso que faz o laço
//     convergir sem uma releitura extra.
// ─────────────────────────────────────────────────────────────────────────────
void demo_cas() {
    fmt::print("\n── 4.1 O laço de compare_exchange ──\n");

    fmt::print("      Forma canônica — \"atualize o máximo\":\n\n");
    fmt::print("        void atualiza_max(std::atomic<int>& m, int v) {{\n");
    fmt::print("            int atual = m.load();\n");
    fmt::print("            while (v > atual &&\n");
    fmt::print("                   !m.compare_exchange_weak(atual, v)) {{\n");
    fmt::print("                // corpo VAZIO: o CAS já reescreveu `atual`\n");
    fmt::print("            }}\n");
    fmt::print("        }}\n\n");

    // Medimos com o exemplo mais contencioso possível: reimplementar
    // `fetch_add` em cima de CAS. Aqui TODA thread precisa disputar o mesmo
    // átomo em toda iteração — não há como uma delas "passar direto".
    std::atomic<long> contador{0};
    std::atomic<long> tentativas{0};

    auto incrementa = [&] {
        long atual = contador.load(std::memory_order_relaxed);
        while (true) {
            tentativas.fetch_add(1, std::memory_order_relaxed);
            if (contador.compare_exchange_weak(atual, atual + 1,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed))
                return;
            // falhou → `atual` já traz o valor real; o laço tenta de novo
        }
    };

    constexpr int kThreads = 4, kPorThread = 20'000;
    {
        std::vector<std::jthread> ts;
        for (int t = 0; t < kThreads; ++t)
            ts.emplace_back([&] { for (int i = 0; i < kPorThread; ++i) incrementa(); });
    }

    const long sucessos = contador.load();
    const long total    = tentativas.load();
    fmt::print("      `fetch_add` reimplementado com CAS, {} threads × {}:\n",
               kThreads, kPorThread);
    fmt::print("        incrementos efetivados : {}\n", sucessos);
    fmt::print("        tentativas de CAS      : {}\n", total);
    fmt::print("        falhas por contenção   : {} ({:.1f}% de retrabalho)\n",
               total - sucessos,
               100.0 * static_cast<double>(total - sucessos)
                     / static_cast<double>(total));
    fmt::print("      Nenhum incremento se perdeu — mas cada falha custou outra\n");
    fmt::print("      volta no laço. É esse retrabalho que faz um algoritmo\n");
    fmt::print("      lock-free degradar sob contenção: ele nunca BLOQUEIA, e\n");
    fmt::print("      ainda assim não é de graça. Com 1 thread o retrabalho é 0%.\n");
    fmt::print("      (na prática use `fetch_add`: o hardware faz num `lock xadd`\n");
    fmt::print("       só, sem laço nenhum)\n");
    fmt::print("      que faz um algoritmo lock-free degradar sob contenção —\n");
    fmt::print("      ele não bloqueia, mas também não é de graça.\n");

    fmt::print("\n      weak vs strong:\n");
    fmt::print("        _weak   pode falhar ESPURIAMENTE (mesmo com o valor certo).\n");
    fmt::print("                Em LL/SC — ARM, POWER — mapeia direto e é barato.\n");
    fmt::print("                Use DENTRO de um laço, onde a falha só custa outra volta.\n");
    fmt::print("        _strong só falha se o valor realmente mudou. Em LL/SC precisa\n");
    fmt::print("                de um laço interno. Use quando NÃO houver laço externo.\n");

    fmt::print("\n      Os dois memory_order do CAS:\n");
    fmt::print("        compare_exchange_weak(esp, novo, ORDEM_SUCESSO, ORDEM_FALHA)\n");
    fmt::print("      A ordem de falha não pode ser mais forte que a de sucesso, e\n");
    fmt::print("      não pode ser `release` (uma falha não escreve nada).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. atomic_flag — o único garantidamente lock-free
// ─────────────────────────────────────────────────────────────────────────────
void demo_flag() {
    fmt::print("\n── 4.2 std::atomic_flag ──\n");

    fmt::print("      É o único tipo que o padrão EXIGE ser lock-free em toda\n");
    fmt::print("      implementação. Tem só três operações:\n");
    fmt::print("        test_and_set()  → põe true, devolve o valor anterior\n");
    fmt::print("        clear()         → põe false\n");
    fmt::print("        test()          → lê sem alterar   (só a partir do C++20)\n\n");

    std::atomic_flag f = ATOMIC_FLAG_INIT;   // em C++20 já inicia limpo sozinho

    fmt::print("      antes:            test() = {}\n", f.test());
    fmt::print("      test_and_set() →  {} (valor ANTERIOR)\n", f.test_and_set());
    fmt::print("      test_and_set() →  {} (agora já estava setado)\n", f.test_and_set());
    fmt::print("      depois do clear:  test() = {}\n", (f.clear(), f.test()));

    fmt::print("\n      Usado como \"faça isto uma vez só\":\n");
    std::atomic_flag ja_inicializou{};
    std::atomic<int> quantas_vezes{0};
    {
        std::vector<std::jthread> ts;
        for (int i = 0; i < 8; ++i)
            ts.emplace_back([&] {
                if (!ja_inicializou.test_and_set())
                    quantas_vezes.fetch_add(1);
            });
    }
    fmt::print("      8 threads competiram; inicializou {} vez(es)\n",
               quantas_vezes.load());
    fmt::print("      ⚠  para inicialização de verdade prefira `std::call_once` ou\n");
    fmt::print("         uma estática local (thread-safe desde o C++11) — elas\n");
    fmt::print("         fazem as OUTRAS threads ESPERAREM. O flag acima só diz\n");
    fmt::print("         quem chegou primeiro; as demais seguem sem esperar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. atomic::wait / notify — bloquear sem mutex (C++20)
//
//     Antes: para dormir esperando uma condição era obrigatório ter um mutex
//     e uma condition_variable. Desde o C++20 todo `std::atomic<T>` tem
//     `wait`/`notify_one`/`notify_all`, implementados sobre futex.
// ─────────────────────────────────────────────────────────────────────────────
void demo_wait_notify() {
    fmt::print("\n── 4.3 atomic::wait / notify (C++20) ──\n");

    std::atomic<int> estado{0};

    std::jthread trabalhador{[&estado] {
        // "durma enquanto o valor AINDA for 0"
        estado.wait(0);                       // não gasta CPU: futex
        fmt::print("      [worker] acordei, estado = {}\n", estado.load());

        estado.store(2);
        estado.notify_all();
    }};

    std::this_thread::sleep_for(15ms);
    fmt::print("      [main] publicando estado = 1\n");
    estado.store(1);
    estado.notify_one();                      // acorda um esperador

    estado.wait(1);                           // agora a main espera virar != 1
    fmt::print("      [main] worker respondeu com estado = {}\n", estado.load());

    fmt::print("\n      A semântica é \"espere ENQUANTO for igual a este valor\" —\n");
    fmt::print("      o inverso da intuição de `cv.wait(lk, pred)`.\n");
    fmt::print("      Também sofre wakeup espúrio: em código real, laço.\n\n");

    fmt::print("      | mecanismo             | precisa de mutex | espera |\n");
    fmt::print("      |-----------------------|------------------|--------|\n");
    fmt::print("      | while (x.load()) {{}}   | não              | SPIN   |\n");
    fmt::print("      | atomic::wait          | não              | dorme  |\n");
    fmt::print("      | condition_variable    | SIM              | dorme  |\n");
    fmt::print("      O `atomic::wait` preenche exatamente o buraco entre os dois.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ spin lock
// ─────────────────────────────────────────────────────────────────────────────
void demo_spinlock() {
    fmt::print("\n── 4.4 ⚠ Spin lock ──\n");

    // Implementação didática — correta, mas veja a discussão abaixo.
    class SpinLock {
        std::atomic_flag travado_{};
    public:
        void lock() {
            while (travado_.test_and_set(std::memory_order_acquire)) {
                // `test()` puro não escreve na linha de cache; ficar em
                // `test_and_set` faria a linha pingar entre os núcleos a cada
                // volta. Isto é o "test and test-and-set".
                while (travado_.test(std::memory_order_relaxed))
                    ;
            }
        }
        void unlock() { travado_.clear(std::memory_order_release); }
    };

    SpinLock sl;
    long soma = 0;
    {
        std::vector<std::jthread> ts;
        for (int t = 0; t < 4; ++t)
            ts.emplace_back([&] {
                for (int i = 0; i < 10'000; ++i) {
                    sl.lock();
                    ++soma;                  // não atômico: o lock protege
                    sl.unlock();
                }
            });
    }
    fmt::print("      4 × 10000 incrementos protegidos por spin lock: {}\n", soma);

    fmt::print("\n      Funciona. E ainda assim, quase sempre é a escolha ERRADA:\n");
    fmt::print("        • quem espera QUEIMA CPU integral — num core que o dono do\n");
    fmt::print("          lock talvez precisasse para terminar e liberar;\n");
    fmt::print("        • se o SO preempta o dono, os outros giram por um\n");
    fmt::print("          quantum inteiro à toa;\n");
    fmt::print("        • sem RAII (o `scoped_lock` do padrão não o conhece a menos\n");
    fmt::print("          que você declare lock/unlock/try_lock);\n");
    fmt::print("        • não é reentrante e não detecta deadlock.\n\n");

    fmt::print("      O `std::mutex` do glibc JÁ faz um spin curto antes de\n");
    fmt::print("      chamar o futex. Ou seja: você ganha o melhor dos dois de\n");
    fmt::print("      graça. Só considere spin lock quando a seção crítica for de\n");
    fmt::print("      pouquíssimas instruções, sem chamadas, e você tiver MEDIDO.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_cas();
    demo_flag();
    demo_wait_notify();
    demo_spinlock();
}

}  // namespace demo_atomicos
