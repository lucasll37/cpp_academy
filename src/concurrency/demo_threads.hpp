// =============================================================================
//  src/concurrency/demo_threads.hpp
//
//  std::thread — criação e ciclo de vida
//  ──────────────────────────────────────
//  Uma thread é uma linha de execução independente dentro do mesmo processo.
//  Compartilha memória com outras threads — isso é tanto a vantagem
//  (comunicação fácil) quanto o perigo (race conditions).
//
//  Tópicos:
//  • Criação com função, lambda e método de objeto
//  • join() vs detach() — e o que acontece se você esquecer
//  • Passagem de argumentos (por valor, por referência com std::ref)
//  • std::this_thread — id, sleep, yield
//  • thread_local — variável separada por thread
//  • RAII wrapper para evitar esquecer o join
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <thread>
#include <vector>
#include <chrono>
#include <string>
#include <sstream>

// std::thread::id não tem formatter nativo no fmt — helper para converter
static std::string thread_id_str(std::thread::id id) {
    std::ostringstream oss;
    oss << id;
    return oss.str();
}

namespace demo_threads {

using namespace std::chrono_literals; // permite escrever 100ms, 1s, etc.

// ─────────────────────────────────────────────────────────────────────────────
//  1. Criação básica — função, lambda, método
// ─────────────────────────────────────────────────────────────────────────────

void tarefa(int id, const std::string& msg) {
    fmt::print("  [thread {}] {}\n", id, msg);
}

struct Trabalhador {
    int id;
    void executar() const {
        fmt::print("  [Trabalhador {}] método executado na thread {}\n",
                   id, thread_id_str(std::this_thread::get_id()));
    }
};

void demo_criacao() {
    fmt::print("\n── 1.1 Criação com função, lambda e método ──\n");

    // Com função livre
    std::thread t1(tarefa, 1, "via função livre");

    // Com lambda — captura por valor é mais seguro (sem dangling reference)
    int dado = 42;
    std::thread t2([dado]() {
        fmt::print("  [lambda] dado capturado = {}\n", dado);
    });

    // Com método de objeto
    Trabalhador w{3};
    std::thread t3(&Trabalhador::executar, &w);

    // join(): bloqueia até a thread terminar — OBRIGATÓRIO antes de destruir
    t1.join();
    t2.join();
    t3.join();

    fmt::print("  todas as threads terminaram\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. join() vs detach() — e o perigo de esquecer
//
//  join()   → espera a thread terminar. Após join(), thread não é joinable.
//  detach() → solta a thread para rodar independentemente (daemon thread).
//             Após detach(), você não controla mais quando ela termina.
//
//  Se um std::thread é destruído enquanto ainda é joinable (sem join nem
//  detach) → std::terminate() é chamado → o programa ABORTA.
// ─────────────────────────────────────────────────────────────────────────────
void demo_join_detach() {
    fmt::print("\n── 1.2 join() vs detach() ──\n");

    // join() — você espera
    {
        std::thread t([]() {
            std::this_thread::sleep_for(10ms);
            fmt::print("  [joinable] terminei\n");
        });
        fmt::print("  aguardando thread...\n");
        t.join();
        fmt::print("  thread concluída, joinable()={}\n", t.joinable());
    }

    // detach() — você solta e não espera
    // ⚠ Cuidado: se a thread acessa memória do caller após o caller sair
    //    de escopo, é undefined behavior. Use apenas para daemons independentes.
    {
        std::thread t([]() {
            std::this_thread::sleep_for(5ms);
            // Não acessamos nada do escopo externo — seguro
        });
        t.detach();
        fmt::print("  thread destacada, joinable()={}\n", t.joinable());
        // A thread pode ainda estar rodando aqui — não temos como saber
    }

    std::this_thread::sleep_for(20ms); // dá tempo para a detached thread terminar
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Passagem de argumentos
//     std::thread COPIA os argumentos por padrão.
//     Para passar por referência: use std::ref() ou std::cref()
// ─────────────────────────────────────────────────────────────────────────────
void incrementar(int& valor, int quantidade) {
    valor += quantidade;
}

void demo_argumentos() {
    fmt::print("\n── 1.3 Passagem de argumentos ──\n");

    // Por valor — thread recebe cópia, modificações não afetam o original
    int x = 10;
    std::thread t1([x]() mutable {  // mutable: permite modificar a cópia
        x += 100;
        fmt::print("  [t1] x dentro da thread = {} (cópia)\n", x);
    });
    t1.join();
    fmt::print("  x no caller após t1 = {} (não mudou)\n", x);

    // Por referência — std::ref() é OBRIGATÓRIO
    // sem std::ref, std::thread tentaria copiar e o compilador rejeitaria
    int resultado = 0;
    std::thread t2(incrementar, std::ref(resultado), 42);
    t2.join();
    fmt::print("  resultado após t2 = {} (modificado via ref)\n", resultado);

    // ⚠ Perigo clássico: capturar referência para variável local em lambda detached
    // std::thread t3([&resultado]() { resultado = 999; }); // OK se joinarmos
    // t3.detach(); // PERIGOSO: resultado pode não existir mais quando t3 rodar
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. std::this_thread — utilitários da thread corrente
// ─────────────────────────────────────────────────────────────────────────────
void demo_this_thread() {
    fmt::print("\n── 1.4 std::this_thread ──\n");

    fmt::print("  thread principal id: {}\n", thread_id_str(std::this_thread::get_id()));

    std::thread t([]() {
        fmt::print("  thread filha id:     {}\n", thread_id_str(std::this_thread::get_id()));

        // sleep_for: dorme por duração
        std::this_thread::sleep_for(10ms);

        // yield: sugere ao SO que outras threads possam rodar agora
        // Útil em spin-wait loops para não desperdiçar CPU
        std::this_thread::yield();

        fmt::print("  thread filha acordou\n");
    });
    t.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. thread_local — cada thread tem sua própria cópia da variável
//     Útil para: contadores por thread, buffers temporários, RNG seeds
// ─────────────────────────────────────────────────────────────────────────────
thread_local int contador_local = 0; // variável separada para cada thread

void incrementar_local(int vezes, const char* nome) {
    for (int i = 0; i < vezes; ++i)
        ++contador_local;
    fmt::print("  [{}] contador_local = {} (exclusivo desta thread)\n",
               nome, contador_local);
}

void demo_thread_local() {
    fmt::print("\n── 1.5 thread_local ──\n");

    // Cada thread tem seu próprio contador_local — sem conflito, sem mutex
    std::thread t1(incrementar_local, 5,  "t1");
    std::thread t2(incrementar_local, 10, "t2");
    std::thread t3(incrementar_local, 3,  "t3");

    t1.join(); t2.join(); t3.join();

    incrementar_local(1, "main");
    fmt::print("  (nenhuma sincronização necessária — sem estado compartilhado)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. RAII wrapper — garantir join mesmo com exceções
//     Se uma exceção for lançada entre criar a thread e chamar join(),
//     o destruidor do std::thread chama terminate().
//     Solução: wrapper que faz join no destruidor.
// ─────────────────────────────────────────────────────────────────────────────
class ThreadGuard {
public:
    explicit ThreadGuard(std::thread t) : t_(std::move(t)) {}

    ~ThreadGuard() {
        if (t_.joinable()) {
            fmt::print("  [ThreadGuard] join automático no destruidor\n");
            t_.join();
        }
    }

    // Não copiável — ownership exclusivo
    ThreadGuard(const ThreadGuard&)            = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;

private:
    std::thread t_;
};

void demo_raii_guard() {
    fmt::print("\n── 1.6 RAII guard para join seguro ──\n");

    {
        ThreadGuard g(std::thread([]() {
            std::this_thread::sleep_for(5ms);
            fmt::print("  [guarded thread] executando\n");
        }));
        fmt::print("  saindo do escopo — guard vai chamar join()\n");
        // join é chamado aqui pelo destruidor de ThreadGuard
        // mesmo que uma exceção seja lançada
    }
    fmt::print("  escopo encerrado com segurança\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_criacao();
    demo_join_detach();
    demo_argumentos();
    demo_this_thread();
    demo_thread_local();
    demo_raii_guard();
}

} // namespace demo_threads