// =============================================================================
//  src/chrono/demo_medicao.hpp
//
//  Como cronometrar sem se enganar
//  ───────────────────────────────
//  Todo demo deste repositório que diz "medido" usa o que está aqui. Este
//  arquivo é o método por trás dos números — e o inventário das formas de
//  produzir números que não significam nada.
//
//  As cinco armadilhas, em ordem de frequência:
//    1. o compilador ELIMINA o código que você quer medir
//    2. o trecho dura menos que a resolução do relógio
//    3. a primeira execução paga cache frio e page faults
//    4. você mede o build de DEBUG e conclui sobre o de RELEASE
//    5. uma execução só — sem repetir, sem mediana
//
//  Tópicos:
//  • ⚠ A eliminação por dead code — e as três defesas
//  • Repetir e dividir, quando o trecho é curto demais
//  • Aquecimento, e por que a primeira medida sempre mente
//  • Mediana em vez de média
//  • Um cronômetro RAII reutilizável
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace demo_medicao {

namespace ch = std::chrono;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. ⚠ A eliminação por dead code
//
//     Se o resultado não é usado, o compilador tem o direito de apagar o
//     cálculo inteiro. Em -O2, um laço que soma um milhão de números e joga o
//     resultado fora simplesmente NÃO EXISTE no binário.
// ─────────────────────────────────────────────────────────────────────────────
long trabalho(int n) {
    long soma = 0;
    for (int i = 0; i < n; ++i) soma += i % 7;
    return soma;
}

// Defesa (a): `volatile` — o compilador não pode eliminar a escrita
// Defesa (b): devolver o valor e o chamador consumir
// Defesa (c): a barreira de otimização (o que o Google Benchmark usa)
template <typename T>
inline void nao_otimize(T&& valor) {
    // Diz ao compilador: "este valor pode ser lido e modificado por código
    // que você não vê". Ele então precisa materializá-lo de verdade.
    asm volatile("" : : "r,m"(valor) : "memory");
}

void demo_eliminacao() {
    fmt::print("\n── 5.1 ⚠ A eliminação por dead code ──\n");

    constexpr int N = 2'000'000;
    using clock = ch::steady_clock;

    // ✗ Sem defesa: o resultado é ignorado
    const auto t0 = clock::now();
    trabalho(N);                                  // resultado descartado
    const auto t_sem = ch::duration_cast<ch::microseconds>(clock::now() - t0).count();

    // ✓ Com volatile
    const auto t1 = clock::now();
    const volatile long r1 = trabalho(N);
    (void)r1;
    const auto t_volatile = ch::duration_cast<ch::microseconds>(clock::now() - t1).count();

    // ✓ Com barreira de otimização
    const auto t2 = clock::now();
    auto r2 = trabalho(N);
    nao_otimize(r2);
    const auto t_barreira = ch::duration_cast<ch::microseconds>(clock::now() - t2).count();

    fmt::print("      somando {} números:\n", N);
    fmt::print("        resultado descartado     : {:>6} µs\n", t_sem);
    fmt::print("        com `volatile`           : {:>6} µs\n", t_volatile);
    fmt::print("        com barreira de otimiz.  : {:>6} µs\n", t_barreira);

    fmt::print("\n      Neste build (-O0) os três são parecidos, porque em -O0 o\n");
    fmt::print("      compilador não elimina nada. Compile com -O2 e a primeira\n");
    fmt::print("      linha vira ~0 µs — o laço inteiro desaparece.\n");
    fmt::print("      Experimente:\n");
    fmt::print("        g++ -O2 ... && ./chrono | grep 'descartado'\n");

    fmt::print("\n      As três defesas, em ordem de preferência:\n");
    fmt::print("        1. barreira de otimização (asm volatile)  ← a correta\n");
    fmt::print("        2. `volatile` na variável de resultado    ← simples\n");
    fmt::print("        3. imprimir/somar o resultado no fim      ← polui a medida\n");
    fmt::print("      ⚠  `volatile` também IMPEDE otimizações legítimas em volta,\n");
    fmt::print("         então a barreira é mais precisa. É por isso que o Google\n");
    fmt::print("         Benchmark tem `DoNotOptimize`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Repetir e dividir
//
//     `steady_clock::now()` custa ~20ns (medido em demo_clocks.hpp, 2.3).
//     Medir algo que dura 5ns com ele é medir o relógio.
// ─────────────────────────────────────────────────────────────────────────────
void demo_repetir() {
    fmt::print("\n── 5.2 Quando o trecho é curto demais ──\n");

    using clock = ch::steady_clock;

    // Uma operação de ~poucos nanossegundos
    volatile int x = 12345;
    const auto operacao = [&x] { return x * 2 + 1; };

    // ✗ Medindo UMA execução
    const auto t0 = clock::now();
    auto r = operacao();
    nao_otimize(r);
    const auto uma = ch::duration_cast<ch::nanoseconds>(clock::now() - t0).count();

    // ✓ Medindo N e dividindo
    constexpr int N = 1'000'000;
    const auto t1 = clock::now();
    for (int i = 0; i < N; ++i) { auto v = operacao(); nao_otimize(v); }
    const auto total = ch::duration_cast<ch::nanoseconds>(clock::now() - t1).count();

    fmt::print("      uma execução medida direto : {} ns\n", uma);
    fmt::print("      {} execuções / N           : {} ns por operação\n",
               N, total / N);
    fmt::print("      ↑ a primeira mede o RELÓGIO (~20ns de overhead), não a\n");
    fmt::print("        operação. A segunda dilui o overhead por N.\n");

    fmt::print("\n      Regra prática: o trecho medido precisa durar pelo menos\n");
    fmt::print("      100× a resolução do relógio. Com ~20ns de overhead, isso\n");
    fmt::print("      significa alvos de 2µs ou mais — ou repetir.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Aquecimento
// ─────────────────────────────────────────────────────────────────────────────
void demo_aquecimento() {
    fmt::print("\n── 5.3 Aquecimento ──\n");

    using clock = ch::steady_clock;
    constexpr std::size_t TAM = 4'000'000;

    // ── Caso A: memória JÁ TOCADA antes de medir ────────────────────────────
    {
        std::vector<int> dados(TAM);
        std::iota(dados.begin(), dados.end(), 0);      // ← toca todas as páginas

        const auto somar = [&dados] {
            long s = 0;
            for (int v : dados) s += v;
            return s;
        };

        std::vector<long> t;
        for (int i = 0; i < 5; ++i) {
            const auto t0 = clock::now();
            auto r = somar();
            nao_otimize(r);
            t.push_back(ch::duration_cast<ch::microseconds>(clock::now() - t0).count());
        }

        fmt::print("      (A) vetor já preenchido por `iota` antes de medir:\n        ");
        for (auto v : t) fmt::print("{} ", v);
        fmt::print("µs\n");
        fmt::print("          → as 5 são parecidas. O `iota` JÁ pagou os page\n");
        fmt::print("            faults e já trouxe os dados para o cache.\n");
    }

    // ── Caso B: memória RECÉM-ALOCADA, nunca tocada ─────────────────────────
    {
        // `make_unique_for_overwrite` NÃO zera — as páginas continuam virgens
        // até o primeiro acesso, e aí o kernel as materializa uma a uma.
        auto bruto = std::make_unique_for_overwrite<unsigned char[]>(TAM);

        std::vector<long> t;
        for (int i = 0; i < 5; ++i) {
            const auto t0 = clock::now();
            unsigned long s = 0;
            for (std::size_t k = 0; k < TAM; ++k) s += bruto[k];
            nao_otimize(s);
            t.push_back(ch::duration_cast<ch::microseconds>(clock::now() - t0).count());
        }

        fmt::print("\n      (B) buffer de {} MB recém-alocado, NUNCA tocado:\n        ",
                   TAM / (1024 * 1024));
        for (auto v : t) fmt::print("{} ", v);
        fmt::print("µs\n");

        const auto primeira = t.front();
        const auto resto = std::accumulate(t.begin() + 1, t.end(), 0L) /
                           static_cast<long>(t.size() - 1);
        if (resto > 0 && primeira > resto) {
            fmt::print("          → a 1ª foi {:.0f}% mais lenta: ela pagou os page\n",
                       100.0 * (primeira - resto) / resto);
            fmt::print("            faults de materializar {} MB de páginas.\n",
                       TAM / (1024 * 1024));
        } else {
            fmt::print("          → aqui o efeito não apareceu — depende de quanto\n");
            fmt::print("            o kernel pré-materializou e do estado do cache.\n");
        }
    }

    fmt::print("\n      A LIÇÃO, que vale mesmo quando o efeito não aparece:\n");
    fmt::print("        a primeira execução mede coisas que as seguintes não\n");
    fmt::print("        medem — page faults, cache frio, preditor de saltos sem\n");
    fmt::print("        histórico, CPU em frequência baixa.\n");
    fmt::print("        Se ela veio IGUAL às outras, é porque algo antes do\n");
    fmt::print("        cronômetro já aqueceu (foi o que aconteceu no caso A).\n");
    fmt::print("      Em qualquer caso: DESCARTE as primeiras execuções. O custo\n");
    fmt::print("      de descartar é zero; o de não descartar é uma conclusão\n");
    fmt::print("      errada.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Mediana, não média
// ─────────────────────────────────────────────────────────────────────────────
void demo_mediana() {
    fmt::print("\n── 5.4 Mediana, não média ──\n");

    using clock = ch::steady_clock;
    std::vector<long> amostras;

    for (int i = 0; i < 21; ++i) {
        const auto t0 = clock::now();
        auto r = trabalho(200'000);
        nao_otimize(r);
        amostras.push_back(ch::duration_cast<ch::microseconds>(clock::now() - t0).count());
    }

    auto ordenado = amostras;
    std::sort(ordenado.begin(), ordenado.end());

    const auto media = std::accumulate(amostras.begin(), amostras.end(), 0L) /
                       static_cast<long>(amostras.size());
    const auto mediana = ordenado[ordenado.size() / 2];
    const auto minimo  = ordenado.front();
    const auto maximo  = ordenado.back();
    const auto p95     = ordenado[static_cast<std::size_t>(ordenado.size() * 0.95)];

    fmt::print("      21 execuções do mesmo trabalho:\n");
    fmt::print("        mínimo  : {:>6} µs   ← o mais próximo do custo REAL\n", minimo);
    fmt::print("        mediana : {:>6} µs   ← robusta a outliers\n", mediana);
    fmt::print("        média   : {:>6} µs   ← puxada por qualquer pico\n", media);
    fmt::print("        p95     : {:>6} µs\n", p95);
    fmt::print("        máximo  : {:>6} µs   ← o escalonador do SO interferiu\n", maximo);

    fmt::print("\n      A média é a pior estatística aqui: um único pico (troca\n");
    fmt::print("      de contexto, interrupção, GC de outro processo) a\n");
    fmt::print("      contamina inteira.\n");
    fmt::print("        • para custo TÍPICO       → mediana\n");
    fmt::print("        • para o melhor caso real → mínimo\n");
    fmt::print("        • para latência de SLA    → p95 / p99\n");
    fmt::print("        • média                   → quase nunca\n");
    fmt::print("      (é a mesma escolha que src/yolo/ faz ao medir inferência)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Um cronômetro RAII reutilizável
// ─────────────────────────────────────────────────────────────────────────────
class Cronometro {
public:
    explicit Cronometro(std::string rotulo)
        : rotulo_(std::move(rotulo)), inicio_(ch::steady_clock::now()) {}

    ~Cronometro() {
        const auto dt = ch::steady_clock::now() - inicio_;
        fmt::print("      [{}] {} µs\n", rotulo_,
                   ch::duration_cast<ch::microseconds>(dt).count());
    }

    Cronometro(const Cronometro&)            = delete;
    Cronometro& operator=(const Cronometro&) = delete;

    ch::steady_clock::duration decorrido() const {
        return ch::steady_clock::now() - inicio_;
    }

private:
    std::string                  rotulo_;
    ch::steady_clock::time_point inicio_;
};

// A versão que devolve o tempo, para quando você quer o número
template <typename F>
ch::nanoseconds medir(F&& f, int repeticoes = 1) {
    // Aquecimento: descarta a primeira
    { auto r = f(); nao_otimize(r); }

    const auto t0 = ch::steady_clock::now();
    for (int i = 0; i < repeticoes; ++i) { auto r = f(); nao_otimize(r); }
    const auto dt = ch::steady_clock::now() - t0;

    return ch::duration_cast<ch::nanoseconds>(dt) / repeticoes;
}

void demo_cronometro() {
    fmt::print("\n── 5.5 Um cronômetro reutilizável ──\n");

    fmt::print("      RAII, imprime ao sair do escopo:\n");
    {
        Cronometro c{"trabalho(1M)"};
        auto r = trabalho(1'000'000);
        nao_otimize(r);
    }

    fmt::print("\n      versão que devolve o número, com aquecimento embutido:\n");
    const auto por_chamada = medir([] { return trabalho(100'000); }, 20);
    fmt::print("      medir(trabalho(100k), 20 rep) = {} ns por chamada\n",
               por_chamada.count());

    fmt::print("\n      O que este cronômetro faz de certo:\n");
    fmt::print("        ✓ usa `steady_clock` (monotônico)\n");
    fmt::print("        ✓ descarta a primeira execução\n");
    fmt::print("        ✓ divide por repetições\n");
    fmt::print("        ✓ aplica barreira de otimização\n");
    fmt::print("      O que ele NÃO faz, e um benchmark sério faria:\n");
    fmt::print("        ✗ estatística (mediana, desvio, intervalo de confiança)\n");
    fmt::print("        ✗ ajustar N automaticamente até estabilizar\n");
    fmt::print("        ✗ fixar a thread numa CPU, desligar turbo boost\n");
    fmt::print("        ✗ detectar interferência do escalonador\n");
    fmt::print("      Para isso: Google Benchmark, nanobench ou Catch2.\n");

    fmt::print("\n      E a regra que resume todas as anteriores:\n");
    fmt::print("      um número sem as FLAGS DE COMPILAÇÃO ao lado não é\n");
    fmt::print("      um resultado — é um boato.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_eliminacao();
    demo_repetir();
    demo_aquecimento();
    demo_mediana();
    demo_cronometro();
}

} // namespace demo_medicao
