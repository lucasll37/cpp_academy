// =============================================================================
//  src/cmake/mpi_anel.cpp
//
//  MPI — paralelismo de memória DISTRIBUÍDA
//  ────────────────────────────────────────
//  O modelo é o oposto do OpenMP: N PROCESSOS independentes, cada um com a
//  sua memória, que só se comunicam por MENSAGENS explícitas. Não há variável
//  compartilhada, não há data race — e não há como um processo simplesmente
//  ler o dado do outro.
//
//  | aspecto        | OpenMP                  | MPI                        |
//  |----------------|-------------------------|----------------------------|
//  | unidade        | thread                  | PROCESSO                   |
//  | memória        | compartilhada           | privada por processo       |
//  | comunicação    | variáveis               | MPI_Send / MPI_Recv        |
//  | escala até     | uma máquina             | milhares de máquinas       |
//  | como se lança  | ./prog                  | mpirun -np 4 ./prog        |
//  | risco típico   | data race               | DEADLOCK de mensagens      |
//
//  Os dois se COMBINAM: MPI entre nós de um cluster, OpenMP dentro de cada
//  nó. É o modelo híbrido padrão em HPC.
//
//  ⚠ NÃO VERIFICADO NESTA MÁQUINA: não há MPI instalado aqui. O CMakeLists.txt
//    detecta a ausência e pula este alvo. Para habilitar:
//        sudo apt install libopenmpi-dev
//        cmake -S . -B build && cmake --build build
//        mpirun -np 4 ./build/mpi_anel
// =============================================================================
#include "nucleo.hpp"

#include <mpi.h>

#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

int main(int argc, char** argv) {
    // ⚠ MPI_Init precisa ser a PRIMEIRA coisa, e MPI_Finalize a última.
    //   Chamar qualquer MPI_* fora desse par é comportamento indefinido.
    MPI_Init(&argc, &argv);

    int rank = 0, tamanho = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);      // quem sou eu (0..N-1)
    MPI_Comm_size(MPI_COMM_WORLD, &tamanho);   // quantos somos

    const std::size_t n_total =
        argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 8'000'000;

    // ── 1. O rank 0 é o "mestre": ele gera os dados ──────────────────────────
    std::vector<float> x_total, y_total;
    if (rank == 0) {
        x_total = nucleo::gerar(n_total, 1);
        y_total = nucleo::gerar(n_total, 2);
        std::printf("MPI: %d processo(s), %zu elementos no total\n",
                    tamanho, n_total);
    }

    // ── 2. Divisão do trabalho ──────────────────────────────────────────────
    //     Cada processo fica com uma FATIA. `MPI_Scatter` exige fatias iguais,
    //     então arredondamos para cima e o rank 0 preenche o resto com zeros.
    const std::size_t por_processo =
        (n_total + static_cast<std::size_t>(tamanho) - 1) /
        static_cast<std::size_t>(tamanho);
    const std::size_t n_ajustado = por_processo * static_cast<std::size_t>(tamanho);

    if (rank == 0) { x_total.resize(n_ajustado, 0.0f); y_total.resize(n_ajustado, 0.0f); }

    std::vector<float> x_local(por_processo), y_local(por_processo);

    // MPI_Scatter: o rank 0 FATIA e distribui; todos recebem a sua parte.
    MPI_Scatter(x_total.data(), static_cast<int>(por_processo), MPI_FLOAT,
                x_local.data(),  static_cast<int>(por_processo), MPI_FLOAT,
                0, MPI_COMM_WORLD);
    MPI_Scatter(y_total.data(), static_cast<int>(por_processo), MPI_FLOAT,
                y_local.data(),  static_cast<int>(por_processo), MPI_FLOAT,
                0, MPI_COMM_WORLD);

    // ── 3. Cada processo calcula a SUA fatia — em paralelo, sem comunicação ──
    const double t0 = MPI_Wtime();             // o cronômetro do MPI
    nucleo::saxpy(2.5f, x_local, y_local);
    const double t_local = MPI_Wtime() - t0;

    // ── 4. Reunir: uma REDUÇÃO e um GATHER ──────────────────────────────────
    const double soma_local = nucleo::soma(y_local);
    double       soma_global = 0.0;

    // MPI_Reduce combina um valor de cada processo com uma operação.
    MPI_Reduce(&soma_local, &soma_global, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    double t_max = 0.0;
    MPI_Reduce(&t_local, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // MPI_Gather traz as fatias de volta para o rank 0.
    MPI_Gather(y_local.data(), static_cast<int>(por_processo), MPI_FLOAT,
               y_total.data(), static_cast<int>(por_processo), MPI_FLOAT,
               0, MPI_COMM_WORLD);

    // ── 5. O anel: comunicação PONTO A PONTO, e o deadlock que espreita ─────
    //     Cada processo manda o seu rank ao vizinho da direita e recebe do da
    //     esquerda. Um `MPI_Send` seguido de `MPI_Recv` em TODOS os processos
    //     ao mesmo tempo pode TRAVAR: se a mensagem for grande demais para o
    //     buffer interno, todo mundo bloqueia esperando alguém receber.
    //     `MPI_Sendrecv` faz os dois numa operação, sem esse risco.
    const int direita  = (rank + 1) % tamanho;
    const int esquerda = (rank - 1 + tamanho) % tamanho;

    int envio = rank, recebido = -1;
    MPI_Sendrecv(&envio,    1, MPI_INT, direita,  0,
                 &recebido, 1, MPI_INT, esquerda, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // ⚠ `printf` de vários processos se INTERCALA de forma imprevisível: os
    //   ranks compartilham o mesmo stdout. Serializamos com barreiras para a
    //   saída ficar legível. Em código de verdade, só o rank 0 imprime.
    for (int r = 0; r < tamanho; ++r) {
        if (r == rank)
            std::printf("  rank %d recebeu %d de %d (anel)\n",
                        rank, recebido, esquerda);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == 0) {
        std::printf("\n  tempo máximo entre os processos : %.2f ms\n", t_max * 1000.0);
        std::printf("  soma global (MPI_Reduce)        : %.6f\n", soma_global);
        std::printf("  soma conferida no rank 0        : %.6f\n",
                    nucleo::soma(std::span<const float>{y_total}));
        std::printf("\n  ⚠ Rodar com -np 1 e -np 4 na MESMA máquina não mede\n");
        std::printf("    escalabilidade: os processos disputam os mesmos núcleos\n");
        std::printf("    e a mesma banda de memória. MPI só mostra o que promete\n");
        std::printf("    quando os processos estão em NÓS diferentes.\n");
    }

    MPI_Finalize();
    return 0;
}
