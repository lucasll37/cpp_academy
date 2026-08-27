// =============================================================================
//  src/concurrency2/demo_lockfree.hpp
//
//  Estruturas lock-free — e por que você provavelmente não quer escrever uma
//  ────────────────────────────────────────────────────────────────────────
//  "Lock-free" não quer dizer "rápido". Quer dizer uma garantia de PROGRESSO:
//
//    bloqueante    — uma thread suspensa no lugar errado trava as outras
//    lock-free     — ALGUMA thread sempre progride (mas uma específica pode
//                    ficar tentando para sempre)
//    wait-free     — TODA thread progride em um número limitado de passos
//
//  A garantia importa em duas situações concretas: código em signal handler
//  (não pode travar mutex) e sistemas de tempo real (o pior caso é o que
//  conta). Fora disso, um `mutex` bem usado costuma ganhar — e este arquivo
//  mede exatamente isso.
//
//  Tópicos:
//  • Fila SPSC — a única lock-free que vale escrever à mão
//  • Pilha lock-free e o problema ABA
//  • ⚠ o vazamento de memória inerente
//  • Medição: lock-free vs mutex, sem e com contenção
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <new>
#include <optional>
#include <thread>
#include <vector>

namespace demo_lockfree {

using namespace std::chrono_literals;

// `hardware_destructive_interference_size` é o tamanho mínimo que separa dois
// objetos para que não caiam na mesma linha de cache. O GCC 13 o oferece, mas
// avisa que o valor pode mudar entre versões e quebrar ABI — por isso o pragma.
// Ver src/performance/demo_false_sharing.hpp para o efeito medido.
#ifdef __cpp_lib_hardware_interference_size
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Winterference-size"
inline constexpr std::size_t kLinha = std::hardware_destructive_interference_size;
#  pragma GCC diagnostic pop
#else
inline constexpr std::size_t kLinha = 64;
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  1. Fila SPSC — Single Producer, Single Consumer
//
//     Com exatamente UM produtor e UM consumidor, o problema fica tratável:
//     o produtor é o único que escreve `cabeca_`, o consumidor é o único que
//     escreve `cauda_`. Não há CAS nenhum — só um par release/acquire em cada
//     lado. É a estrutura lock-free que realmente compensa escrever.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, std::size_t N>
class FilaSPSC {
    static_assert((N & (N - 1)) == 0, "N precisa ser potência de 2");

    // Cada índice na sua própria linha de cache: sem isso, o produtor
    // escrevendo `cabeca_` invalidaria a linha do consumidor a cada push.
    alignas(kLinha) std::atomic<std::size_t> cabeca_{0};   // só o produtor escreve
    alignas(kLinha) std::atomic<std::size_t> cauda_{0};    // só o consumidor escreve
    alignas(kLinha) T                        buf_[N]{};

public:
    bool push(const T& v) {
        const std::size_t c = cabeca_.load(std::memory_order_relaxed);
        const std::size_t prox = (c + 1) & (N - 1);
        // acquire: precisa enxergar os pops já feitos pelo consumidor
        if (prox == cauda_.load(std::memory_order_acquire)) return false;  // cheia
        buf_[c] = v;
        cabeca_.store(prox, std::memory_order_release);   // publica o dado junto
        return true;
    }

    std::optional<T> pop() {
        const std::size_t t = cauda_.load(std::memory_order_relaxed);
        if (t == cabeca_.load(std::memory_order_acquire)) return std::nullopt;  // vazia
        T v = buf_[t];
        cauda_.store((t + 1) & (N - 1), std::memory_order_release);
        return v;
    }
};

void demo_spsc() {
    fmt::print("\n── 5.1 Fila SPSC ──\n");

    FilaSPSC<int, 1024> fila;
    constexpr int kItens = 200'000;

    std::atomic<long> soma{0};
    std::atomic<int>  recebidos{0};

    const auto t0 = std::chrono::steady_clock::now();
    {
        // ⚠ NÃO use `stop_token` aqui. O destrutor do `jthread` chama
        //   `request_stop()` ANTES do `join()`; o consumidor sairia com a fila
        //   ainda cheia e o demo perderia itens — foi exatamente o que
        //   aconteceu na primeira versão deste arquivo. A condição de parada
        //   correta é o trabalho ter TERMINADO, não a parada ter sido pedida.
        //   É a armadilha descrita em demo_jthread.hpp, 1.3.
        std::jthread consumidor{[&] {
            while (recebidos.load(std::memory_order_relaxed) < kItens) {
                if (auto v = fila.pop()) {
                    soma.fetch_add(*v, std::memory_order_relaxed);
                    recebidos.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();     // fila vazia: cede o núcleo
                }
            }
        }};

        for (int i = 1; i <= kItens; ++i)
            while (!fila.push(i)) std::this_thread::yield();   // cheia: espera
    }
    const auto t1 = std::chrono::steady_clock::now();

    const long esperado = 1L * kItens * (kItens + 1) / 2;
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    fmt::print("      {} itens em {:.1f} ms ({:.1f} M itens/s)\n",
               kItens, ms, kItens / ms / 1000.0);
    fmt::print("      soma recebida = {} (esperada {}) — {}\n",
               soma.load(), esperado,
               soma.load() == esperado ? "íntegra" : "PERDEU DADOS");
    fmt::print("      Zero CAS, zero mutex: só dois `store(release)` e dois\n");
    fmt::print("      `load(acquire)`. O `alignas` em cada índice evita false\n");
    fmt::print("      sharing entre as duas threads.\n");

    fmt::print("\n      ⚠  a garantia depende de ser 1 produtor e 1 consumidor.\n");
    fmt::print("         Com dois produtores, dois podem ler a mesma `cabeca_`,\n");
    fmt::print("         escrever no mesmo slot e um sobrescrever o outro.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Pilha lock-free e o problema ABA
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
class PilhaLockFree {
    struct No {
        T   valor;
        No* prox;
    };
    std::atomic<No*> topo_{nullptr};

public:
    void push(T v) {
        No* n = new No{std::move(v), nullptr};
        n->prox = topo_.load(std::memory_order_relaxed);
        // se outra thread trocou o topo, `n->prox` é reatualizado e tentamos de novo
        while (!topo_.compare_exchange_weak(n->prox, n,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) { }
    }

    std::optional<T> pop() {
        No* t = topo_.load(std::memory_order_acquire);
        while (t && !topo_.compare_exchange_weak(t, t->prox,
                                                 std::memory_order_acquire,
                                                 std::memory_order_relaxed)) { }
        if (!t) return std::nullopt;
        T v = std::move(t->valor);
        // ⚠ ATENÇÃO: este `delete` é UM BUG. Ver a discussão em 5.3.
        //   Está aqui para NÃO vazar no demo; num pop concorrente de verdade
        //   outra thread pode estar lendo `t->prox` neste exato instante.
        delete t;
        return v;
    }

    ~PilhaLockFree() {
        while (No* t = topo_.load()) { topo_.store(t->prox); delete t; }
    }
};

void demo_pilha() {
    fmt::print("\n── 5.2 Pilha lock-free e o ABA ──\n");

    PilhaLockFree<int> pilha;
    constexpr int kThreads = 4, kPorThread = 5'000;

    {   // só pushes concorrentes: seguro, não há leitura de nó liberado
        std::vector<std::jthread> ts;
        for (int t = 0; t < kThreads; ++t)
            ts.emplace_back([&, t] {
                for (int i = 0; i < kPorThread; ++i) pilha.push(t * kPorThread + i);
            });
    }

    int contados = 0;
    long soma = 0;
    while (auto v = pilha.pop()) { ++contados; soma += *v; }

    const int  n = kThreads * kPorThread;
    const long esperado = 1L * n * (n - 1) / 2;
    fmt::print("      {} pushes concorrentes → {} pops, soma {} ({})\n",
               n, contados, soma, soma == esperado ? "íntegra" : "ERRO");

    fmt::print("\n      O problema ABA:\n");
    fmt::print("        thread 1: lê topo = A, vai chamar CAS(A → A->prox = B)\n");
    fmt::print("        thread 2: pop A, pop B, push A de volta\n");
    fmt::print("        thread 1: CAS(A → B) SUCEDE — o ponteiro é A de novo!\n");
    fmt::print("        Mas B já saiu da pilha. O topo agora aponta para lixo.\n\n");
    fmt::print("      O CAS compara ENDEREÇOS, não identidades. E o alocador\n");
    fmt::print("      adora reciclar o endereço que acabou de ser liberado, o\n");
    fmt::print("      que torna o ABA muito mais provável do que parece.\n\n");
    fmt::print("      Defesas conhecidas, todas caras:\n");
    fmt::print("        • contador de versão junto do ponteiro (precisa de CAS\n");
    fmt::print("          de 128 bits: `cmpxchg16b`, e de alinhamento a 16);\n");
    fmt::print("        • hazard pointers — cada thread publica o que está lendo;\n");
    fmt::print("        • RCU / epoch-based reclamation;\n");
    fmt::print("        • simplesmente NUNCA liberar (pool que só cresce).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ O problema da liberação de memória
// ─────────────────────────────────────────────────────────────────────────────
void demo_liberacao() {
    fmt::print("\n── 5.3 ⚠ Quem libera o nó? ──\n");

    fmt::print("      No `pop()` acima:\n");
    fmt::print("        No* t = topo_.load();\n");
    fmt::print("        while (t && !topo_.compare_exchange_weak(t, t->prox)) {{ }}\n");
    fmt::print("                                          ▲\n");
    fmt::print("                            lê t->prox — e se outra thread já\n");
    fmt::print("                            deu `delete t` aqui? use-after-free.\n\n");

    fmt::print("      Este é o calcanhar de Aquiles das estruturas lock-free.\n");
    fmt::print("      Não há momento seguro para liberar um nó, porque não há\n");
    fmt::print("      como saber se alguma thread ainda tem um ponteiro para ele.\n\n");

    fmt::print("      A demo de 5.2 escapa porque separa as fases: primeiro só\n");
    fmt::print("      pushes concorrentes, depois só pops de UMA thread. Com pops\n");
    fmt::print("      concorrentes, o `delete` daquele código é use-after-free —\n");
    fmt::print("      e o ASan acusa. Está marcado no fonte com um ⚠.\n\n");

    fmt::print("      `atomic<shared_ptr<T>>` (C++20) resolve o ciclo de vida e\n");
    fmt::print("      o GCC 13 o tem — mas `is_always_lock_free` é false, então\n");
    fmt::print("      internamente há um mutex. Fica correto, deixa de ser lock-free.\n\n");

    fmt::print("      Conclusão prática: use as prontas.\n");
    fmt::print("        • `moodycamel::ConcurrentQueue` (MPMC, cabeçalho único)\n");
    fmt::print("        • `boost::lockfree::{{queue,stack,spsc_queue}}`\n");
    fmt::print("        • `folly::MPMCQueue`\n");
    fmt::print("      Escrever a sua só se compensa depois de medir, e mesmo então\n");
    fmt::print("      só a SPSC de 5.1 é simples o bastante para revisar com calma.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Medição: lock-free vs mutex
// ─────────────────────────────────────────────────────────────────────────────
void demo_medicao() {
    fmt::print("\n── 5.4 lock-free vs mutex, medido ──\n");

    constexpr int kThreads = 4, kOps = 100'000;

    auto medir = [](auto&& corpo) {
        const auto t0 = std::chrono::steady_clock::now();
        {
            std::vector<std::jthread> ts;
            for (int t = 0; t < kThreads; ++t)
                ts.emplace_back([&] { for (int i = 0; i < kOps; ++i) corpo(); });
        }
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    };

    // (a) contador atômico vs contador sob mutex
    std::atomic<long> at{0};
    long               sob_mutex = 0;
    std::mutex         m;

    const double t_atomic = medir([&] { at.fetch_add(1, std::memory_order_relaxed); });
    const double t_mutex  = medir([&] { std::scoped_lock lk{m}; ++sob_mutex; });

    fmt::print("      contador, {} threads × {} incrementos:\n", kThreads, kOps);
    fmt::print("        atomic fetch_add : {:7.2f} ms\n", t_atomic);
    fmt::print("        mutex + ++       : {:7.2f} ms  ({:.1f}× mais lento)\n",
               t_mutex, t_mutex / t_atomic);
    fmt::print("      Aqui o atômico ganha com folga: a seção crítica é uma\n");
    fmt::print("      instrução, e o mutex adiciona duas operações atômicas\n");
    fmt::print("      (lock e unlock) em volta dela.\n\n");

    // (b) trabalho maior dentro da seção crítica
    std::vector<int> v_mutex;  v_mutex.reserve(kThreads * kOps);
    PilhaLockFree<int> pilha;

    const double t_vec = medir([&] { std::scoped_lock lk{m}; v_mutex.push_back(1); });
    const double t_lf  = medir([&] { pilha.push(1); });

    fmt::print("      inserção em pilha/vetor, {} threads × {}:\n", kThreads, kOps);
    fmt::print("        mutex + vector::push_back : {:7.2f} ms\n", t_vec);
    fmt::print("        pilha lock-free (com new) : {:7.2f} ms\n", t_lf);

    if (t_lf > t_vec) {
        fmt::print("      → o mutex GANHOU. A pilha lock-free paga um `new` por\n");
        fmt::print("        nó (que tem o seu próprio lock interno no alocador) e\n");
        fmt::print("        sofre com o retrabalho do CAS sob contenção.\n");
    } else {
        fmt::print("      → a lock-free ganhou nesta execução; repita algumas\n");
        fmt::print("        vezes, o resultado costuma oscilar.\n");
    }

    fmt::print("\n      A conclusão que interessa: \"lock-free\" é uma garantia de\n");
    fmt::print("      PROGRESSO, não de VELOCIDADE. Escolha lock-free quando\n");
    fmt::print("      precisar da garantia — signal handler, tempo real, ou um\n");
    fmt::print("      caminho onde suspender uma thread é inaceitável. Para ganhar\n");
    fmt::print("      desempenho, primeiro tente reduzir a CONTENÇÃO: dados por\n");
    fmt::print("      thread, sharding, ou simplesmente uma seção crítica menor.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_spsc();
    demo_pilha();
    demo_liberacao();
    demo_medicao();
}

}  // namespace demo_lockfree
