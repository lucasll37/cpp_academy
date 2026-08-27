// =============================================================================
//  src/performance/demo_aos_soa.hpp
//
//  AoS vs SoA — a mesma informação, dois layouts
//  ─────────────────────────────────────────────
//  Array of Structs (AoS) é como quase todo mundo modela, e é a forma mais
//  legível:
//      struct Particula { float x, y, z, vx, vy, vz; float massa; ... };
//      std::vector<Particula> mundo;
//
//  Struct of Arrays (SoA) inverte:
//      struct Mundo { std::vector<float> x, y, z, vx, vy, vz, massa; };
//
//  A diferença aparece quando um laço usa SÓ ALGUNS campos. Em AoS, ler `x`
//  traz `y, z, vx, ...` na mesma linha de cache — e joga fora. Em SoA, a
//  linha inteira é de `x`.
//
//  ⚠ SoA não é melhor. É melhor PARA UM PADRÃO DE ACESSO. Este arquivo mede
//    os dois padrões, e um deles inverte o resultado.
//
//  Tópicos:
//  • O laço que usa poucos campos → SoA ganha
//  • O laço que usa todos os campos → AoS ganha
//  • A vetorização, que só o SoA permite
//  • ⚠ O custo de legibilidade, e o meio-termo
// =============================================================================
#pragma once
#include "bench.hpp"
#include <fmt/core.h>
#include <cstddef>
#include <vector>

namespace demo_aos_soa {

constexpr std::size_t kN = 4'000'000;

// ── AoS ─────────────────────────────────────────────────────────────────────
struct Particula {
    float x, y, z;
    float vx, vy, vz;
    float massa;
    float carga;
    int   id;
    bool  ativa;
};

// ── SoA ─────────────────────────────────────────────────────────────────────
struct Mundo {
    std::vector<float> x, y, z, vx, vy, vz, massa, carga;
    std::vector<int>   id;
    std::vector<char>  ativa;

    explicit Mundo(std::size_t n)
        : x(n, 1.0f), y(n, 1.0f), z(n, 1.0f),
          vx(n, 0.5f), vy(n, 0.5f), vz(n, 0.5f),
          massa(n, 2.0f), carga(n, 1.0f), id(n, 0), ativa(n, 1) {}
};

void demo_poucos_campos() {
    fmt::print("\n── 2.1 Laço que usa POUCOS campos ──\n");

    std::vector<Particula> aos(kN, Particula{1,1,1, 0.5f,0.5f,0.5f, 2, 1, 0, true});
    Mundo soa{kN};

    fmt::print("      sizeof(Particula) = {} bytes → cabem {} numa linha de 64\n",
               sizeof(Particula), 64 / sizeof(Particula));

    // O laço mais comum de uma simulação: integrar a posição.
    // Usa 6 dos 10 campos... não: usa x,y,z,vx,vy,vz = 6 de 40 bytes úteis.
    // Vamos com um laço que usa MENOS: somar só a massa.
    const double t_aos = bench::mediana_ms([&] {
        float s = 0;
        for (const auto& p : aos) s += p.massa;
        bench::nao_otimize(s);
    }, 5);

    const double t_soa = bench::mediana_ms([&] {
        float s = 0;
        for (float m : soa.massa) s += m;
        bench::nao_otimize(s);
    }, 5);

    fmt::print("\n      Somando APENAS `massa` de {} partículas:\n", kN);
    fmt::print("        AoS : {:7.2f} ms\n", t_aos);
    fmt::print("        SoA : {:7.2f} ms   ({:.1f}× mais rápido)\n",
               t_soa, t_aos / t_soa);

    fmt::print("\n      Bytes que o processador teve que trazer da RAM:\n");
    fmt::print("        AoS : {} MB (a struct inteira, {} bytes por partícula)\n",
               kN * sizeof(Particula) / (1024 * 1024), sizeof(Particula));
    fmt::print("        SoA : {} MB (só o vetor de massa, 4 bytes)\n",
               kN * sizeof(float) / (1024 * 1024));
    fmt::print("      A razão de BYTES é {:.0f}×, e a de TEMPO é só {:.1f}×.\n",
               static_cast<double>(sizeof(Particula)) / sizeof(float),
               t_aos / t_soa);
    fmt::print("      A diferença entre esses dois números é o PREFETCHER: o\n");
    fmt::print("      acesso do AoS continua sequencial, e o hardware busca as\n");
    fmt::print("      linhas antes de serem pedidas. Ele não elimina o tráfego —\n");
    fmt::print("      elimina a ESPERA por ele, até saturar a banda de memória.\n");
    fmt::print("      É por isso que \"contar bytes\" prevê o teto, não o tempo.\n");
}

void demo_todos_campos() {
    fmt::print("\n── 2.2 ⚠ Laço que usa TODOS os campos ──\n");

    std::vector<Particula> aos(kN, Particula{1,1,1, 0.5f,0.5f,0.5f, 2, 1, 0, true});
    Mundo soa{kN};

    const double t_aos = bench::mediana_ms([&] {
        float s = 0;
        for (auto& p : aos) {
            p.x += p.vx; p.y += p.vy; p.z += p.vz;
            s += p.x + p.y + p.z + p.massa + p.carga;
        }
        bench::nao_otimize(s);
    }, 5);

    const double t_soa = bench::mediana_ms([&] {
        float s = 0;
        for (std::size_t i = 0; i < kN; ++i) {
            soa.x[i] += soa.vx[i]; soa.y[i] += soa.vy[i]; soa.z[i] += soa.vz[i];
            s += soa.x[i] + soa.y[i] + soa.z[i] + soa.massa[i] + soa.carga[i];
        }
        bench::nao_otimize(s);
    }, 5);

    fmt::print("      Atualizando posição e somando 5 campos:\n");
    fmt::print("        AoS : {:7.2f} ms\n", t_aos);
    fmt::print("        SoA : {:7.2f} ms\n", t_soa);

    if (t_aos < t_soa) {
        fmt::print("      → o AoS GANHOU ({:.2f}×): a struct inteira é útil,\n",
                   t_soa / t_aos);
        fmt::print("        nada é desperdiçado, e há UM fluxo em vez de oito.\n");
    } else {
        fmt::print("      → o SoA ainda ganhou ({:.2f}×).\n", t_aos / t_soa);
        fmt::print("\n      ⚠  Este resultado contraria a expectativa com que o\n");
        fmt::print("         demo foi escrito. A hipótese era que, usando quase\n");
        fmt::print("         todos os campos, o AoS venceria — e ele não venceu.\n");
        fmt::print("         Duas razões prováveis, nesta máquina:\n");
        fmt::print("           • `sizeof(Particula)` é {}, e 64/{} = 1: cada\n",
                   sizeof(Particula), sizeof(Particula));
        fmt::print("             partícula já ocupa mais de uma linha, então o AoS\n");
        fmt::print("             não colhe o benefício de \"tudo na mesma linha\";\n");
        fmt::print("           • o laço SoA vetoriza; o AoS, com passo de {} bytes,\n",
                   sizeof(Particula));
        fmt::print("             não.\n");
        fmt::print("         Para o AoS ganhar, a struct precisaria caber\n");
        fmt::print("         confortavelmente numa linha — o que remete a\n");
        fmt::print("         src/layout/: reordenar campos e cortar padding é\n");
        fmt::print("         pré-requisito, não detalhe.\n");
    }

    fmt::print("\n      A moral: SoA não é \"o layout rápido\". Ele troca\n");
    fmt::print("      DESPERDÍCIO DE LINHA por MAIS FLUXOS SIMULTÂNEOS. O\n");
    fmt::print("      processador tem um número limitado de streams que o\n");
    fmt::print("      prefetcher acompanha (tipicamente 8 a 16); oito vetores\n");
    fmt::print("      percorridos em paralelo já disputam esse recurso.\n");
}

void demo_vetorizacao() {
    fmt::print("\n── 2.3 A vetorização ──\n");

    fmt::print("      Só o SoA permite ao compilador gerar SIMD sobre um campo:\n\n");
    fmt::print("        for (float m : soa.massa) s += m;\n");
    fmt::print("        → o compilador emite `vaddps` sobre 8 floats de uma vez\n\n");
    fmt::print("        for (const auto& p : aos) s += p.massa;\n");
    fmt::print("        → as massas estão a {} bytes de distância uma da outra;\n",
               sizeof(Particula));
    fmt::print("          carregar 8 delas num registrador exigiria um GATHER,\n");
    fmt::print("          que é lento e nem sempre é escolhido.\n\n");

    fmt::print("      Para ver se o compilador vetorizou:\n");
    fmt::print("        g++ -O3 -fopt-info-vec-optimized arq.cpp\n");
    fmt::print("        g++ -O3 -fopt-info-vec-missed arq.cpp   ← e por que NÃO\n\n");

    fmt::print("      ⚠  -O2 no GCC 12+ já faz alguma vetorização; -O3 faz mais.\n");
    fmt::print("         `-march=native` libera AVX2/AVX-512 — e produz um\n");
    fmt::print("         binário que não roda em máquina mais antiga.\n");
}

void demo_meio_termo() {
    fmt::print("\n── 2.4 ⚠ O custo, e o meio-termo ──\n");

    fmt::print("      SoA custa LEGIBILIDADE, e o preço é real:\n\n");
    fmt::print("        AoS : mundo[i].x += mundo[i].vx;\n");
    fmt::print("        SoA : mundo.x[i] += mundo.vx[i];\n\n");
    fmt::print("      Parece pouco, mas some a entidade como OBJETO: não existe\n");
    fmt::print("      mais um `Particula&` para passar adiante. Funções que\n");
    fmt::print("      operavam sobre uma partícula passam a receber um índice e\n");
    fmt::print("      uma referência ao mundo inteiro.\n\n");

    fmt::print("      Os meios-termos, em ordem de esforço:\n\n");
    fmt::print("      (a) SEPARAR POR TEMPERATURA — campos quentes numa struct,\n");
    fmt::print("          frios em outra:\n");
    fmt::print("            struct Quente {{ float x, y, z, vx, vy, vz; }};\n");
    fmt::print("            struct Fria   {{ std::string nome; int id; ... }};\n");
    fmt::print("            std::vector<Quente> q;  std::vector<Fria> f;\n");
    fmt::print("          Mantém a legibilidade e resolve a maior parte.\n\n");

    fmt::print("      (b) AoSoA — blocos de N em SoA, num array:\n");
    fmt::print("            struct Bloco {{ float x[8], y[8], z[8]; }};\n");
    fmt::print("            std::vector<Bloco> mundo;\n");
    fmt::print("          Vetoriza bem E mantém localidade. É o layout de\n");
    fmt::print("          engines de física e de código HPC.\n\n");

    fmt::print("      (c) uma VISÃO sobre o SoA — um proxy que se parece com\n");
    fmt::print("          `Particula&` e por baixo indexa os vetores. Recupera a\n");
    fmt::print("          sintaxe ao custo de mais código de infraestrutura.\n\n");

    fmt::print("      E a regra que vale antes de tudo: só reorganize o layout\n");
    fmt::print("      DEPOIS que o profiler apontar aquele laço. AoS legível é o\n");
    fmt::print("      ponto de partida certo — ver src/ecs/, que já usa SoA por\n");
    fmt::print("      ser a arquitetura natural de um ECS.\n");
}

inline void run() {
    demo_poucos_campos();
    demo_todos_campos();
    demo_vetorizacao();
    demo_meio_termo();
}

}  // namespace demo_aos_soa
