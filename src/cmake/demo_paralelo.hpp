// =============================================================================
//  src/cmake/demo_paralelo.hpp
//
//  OpenMP, MPI e CUDA — três modelos de paralelismo
//  ────────────────────────────────────────────────
//  Os três resolvem o mesmo problema em escalas diferentes, e a diferença
//  entre eles é ONDE a memória mora:
//
//    OpenMP  → N threads, UMA memória, UMA máquina
//    MPI     → N processos, N memórias, MUITAS máquinas
//    CUDA    → dezenas de milhares de threads, memória SEPARADA no dispositivo
//
//  Do lado do CMake, os três seguem a mesma forma: um alvo importado que
//  carrega flags, includes e bibliotecas — `OpenMP::OpenMP_CXX`,
//  `MPI::MPI_CXX`, e a linguagem `CUDA` habilitada.
//
//  ⚠ Este cabeçalho NARRA. Os programas de verdade são alvos separados:
//    omp_saxpy.cpp, mpi_anel.cpp e cuda_saxpy.cu — os três calculam o MESMO
//    SAXPY, o que torna a comparação honesta.
//
//  Tópicos:
//  • Os três modelos, lado a lado
//  • OpenMP: as diretivas que importam, e o resultado MEDIDO
//  • MPI: o modelo de mensagens, e o deadlock
//  • CUDA: host vs device, e a mentira comum de benchmark
// =============================================================================
#pragma once
#include "config.hpp"

#include <cstdio>

#ifdef _OPENMP
#  include <omp.h>
#endif

namespace demo_paralelo {

inline void demo_comparacao() {
    std::printf("\n── 4.1 Os três modelos ──\n");

    std::printf("      | aspecto       | OpenMP        | MPI            | CUDA            |\n");
    std::printf("      |---------------|---------------|----------------|-----------------|\n");
    std::printf("      | unidade       | thread        | PROCESSO       | thread de GPU   |\n");
    std::printf("      | quantidade    | dezenas       | dezenas/milhar | DEZENAS DE MIL  |\n");
    std::printf("      | memória       | compartilhada | privada        | separada (VRAM) |\n");
    std::printf("      | comunicação   | variáveis     | mensagens      | cópias H<->D    |\n");
    std::printf("      | escala até    | 1 máquina     | um cluster     | 1 GPU (ou N)    |\n");
    std::printf("      | como se lança | ./prog        | mpirun -np 4   | ./prog          |\n");
    std::printf("      | risco típico  | data race     | DEADLOCK       | erro silencioso |\n");
    std::printf("      | no CMake      | OpenMP::      | MPI::MPI_CXX   | linguagem CUDA  |\n");
    std::printf("      |               | OpenMP_CXX    |                | + .cu           |\n\n");

    std::printf("      Eles se COMBINAM, e é o padrão em HPC: MPI entre os nós\n");
    std::printf("      do cluster, OpenMP dentro de cada nó, CUDA nas GPUs de\n");
    std::printf("      cada nó. O modelo \"MPI+X\".\n\n");

    std::printf("      Nesta configuração:\n");
    std::printf("        OpenMP : %s\n", config::kTemOpenMP ? "disponível" : "AUSENTE");
    std::printf("        MPI    : %s\n", config::kTemMPI    ? "disponível"
                                       : "AUSENTE (sudo apt install libopenmpi-dev)");
    std::printf("        CUDA   : %s\n", config::kTemCUDA   ? "disponível"
                                       : "AUSENTE (sem nvcc; sudo apt install nvidia-cuda-toolkit)");
}

inline void demo_openmp() {
    std::printf("\n── 4.2 OpenMP ──\n");

#ifdef _OPENMP
    std::printf("      Ativo: versão %d, %d threads disponíveis.\n",
                _OPENMP, omp_get_max_threads());
#else
    std::printf("      ⚠ Este binário foi compilado SEM -fopenmp.\n");
#endif

    std::printf("\n      As quatro diretivas que resolvem 90%% dos casos:\n\n");
    std::printf("        #pragma omp parallel for\n");
    std::printf("            for (long i = 0; i < n; ++i) y[i] = a*x[i] + y[i];\n\n");
    std::printf("        #pragma omp parallel for reduction(+ : soma)\n");
    std::printf("            for (long i = 0; i < n; ++i) soma += v[i];\n\n");
    std::printf("        #pragma omp critical      // seção exclusiva\n");
    std::printf("        #pragma omp atomic        // uma operação, mais barato\n\n");

    std::printf("      ⚠ Sem `reduction`, o segundo laço seria DATA RACE: todas\n");
    std::printf("        as threads escrevendo em `soma`. Compila, roda, e dá um\n");
    std::printf("        resultado diferente a cada execução — UB, não \"erro de\n");
    std::printf("        arredondamento\". Ver src/undefined_behavior/.\n\n");

    std::printf("      `schedule`, e quando cada um:\n");
    std::printf("        static   → fatias iguais, decididas antes. O padrão, e\n");
    std::printf("                   o certo quando toda iteração custa o mesmo.\n");
    std::printf("        dynamic  → cada thread pega a próxima fatia ao terminar.\n");
    std::printf("                   Para custo IRREGULAR (Mandelbrot, ver\n");
    std::printf("                   src/mandelbrot_openmp/).\n");
    std::printf("        guided   → fatias que encolhem; meio-termo.\n");
    std::printf("      ⚠ `dynamic` tem overhead de coordenação por fatia. Usá-lo\n");
    std::printf("        num laço uniforme costuma PIORAR.\n\n");

    std::printf("      ⚠ A armadilha nº 1: o código compila SEM -fopenmp e roda\n");
    std::printf("        SERIAL, em silêncio. Os pragmas viram comentário. Um\n");
    std::printf("        build mal configurado não FALHA — só fica lento. Por\n");
    std::printf("        isso o CMakeLists imprime se achou OpenMP, e o\n");
    std::printf("        omp_saxpy.cpp checa `#ifdef _OPENMP` e avisa.\n\n");

    std::printf("      O RESULTADO MEDIDO, rodando `./build/omp_saxpy`:\n\n");
    std::printf("         elementos |       MB |   serial |   OpenMP | speedup\n");
    std::printf("        -----------|----------|----------|----------|--------\n");
    std::printf("            250000 |        3 |     0.06 |     0.02 |   2.60x\n");
    std::printf("           2000000 |       23 |     0.52 |     0.17 |   3.09x\n");
    std::printf("          20000000 |      229 |     7.01 |     5.50 |   1.27x\n");
    std::printf("          60000000 |      687 |    22.10 |    18.49 |   1.20x\n\n");

    std::printf("      O speedup CAI conforme os dados crescem, com 24 threads\n");
    std::printf("      disponíveis o tempo todo. SAXPY faz 1 multiplicação-soma\n");
    std::printf("      para cada 12 bytes movidos: é limitado por BANDA DE\n");
    std::printf("      MEMÓRIA, não por CPU.\n");
    std::printf("      Enquanto cabe no cache, as threads somam a banda dos\n");
    std::printf("      caches. Quando vem da RAM, todas disputam os MESMOS\n");
    std::printf("      canais — e acrescentar thread não acrescenta banda.\n");
    std::printf("      Ver src/performance/demo_cache.hpp.\n");
}

inline void demo_mpi() {
    std::printf("\n── 4.3 MPI ──\n");

    std::printf("      O modelo é o OPOSTO do OpenMP: N processos, cada um com\n");
    std::printf("      a SUA memória. Não há variável compartilhada, não há data\n");
    std::printf("      race — e não há como um processo ler o dado do outro sem\n");
    std::printf("      pedir.\n\n");

    std::printf("      O esqueleto de todo programa MPI:\n");
    std::printf("        MPI_Init(&argc, &argv);              ← SEMPRE primeiro\n");
    std::printf("        MPI_Comm_rank(MPI_COMM_WORLD, &rank);  quem sou (0..N-1)\n");
    std::printf("        MPI_Comm_size(MPI_COMM_WORLD, &n);     quantos somos\n");
    std::printf("        ...\n");
    std::printf("        MPI_Finalize();                      ← SEMPRE último\n\n");

    std::printf("      As operações COLETIVAS, que fazem o trabalho pesado:\n");
    std::printf("        MPI_Bcast    um envia o mesmo dado a todos\n");
    std::printf("        MPI_Scatter  um FATIA e distribui\n");
    std::printf("        MPI_Gather   o inverso: reúne as fatias\n");
    std::printf("        MPI_Reduce   combina (soma, máximo...) num processo\n");
    std::printf("        MPI_Allreduce  o mesmo, e TODOS recebem o resultado\n");
    std::printf("        MPI_Barrier  todos esperam todos\n\n");

    std::printf("      ⚠ TODOS os processos precisam chamar a coletiva, com os\n");
    std::printf("        MESMOS argumentos de tamanho. Um `if (rank == 0)` em\n");
    std::printf("        volta de um `MPI_Bcast` trava o programa inteiro — os\n");
    std::printf("        outros ficam esperando para sempre.\n\n");

    std::printf("      ⚠ O DEADLOCK ponto a ponto, o erro clássico:\n");
    std::printf("          MPI_Send(...);   // todos enviam\n");
    std::printf("          MPI_Recv(...);   // e só depois recebem\n");
    std::printf("        Se a mensagem couber no buffer interno, funciona. Se\n");
    std::printf("        crescer, TODOS bloqueiam no Send esperando alguém\n");
    std::printf("        receber. O bug aparece só com dados grandes — em\n");
    std::printf("        produção. Use `MPI_Sendrecv`, ou o par não-bloqueante\n");
    std::printf("        `MPI_Isend`/`MPI_Irecv` + `MPI_Wait`.\n\n");

    std::printf("      ⚠ Rodar `-np 1` e `-np 4` na MESMA máquina não mede\n");
    std::printf("        escalabilidade: os processos disputam os mesmos núcleos\n");
    std::printf("        e a mesma banda. MPI só mostra o que promete entre NÓS.\n\n");

    if (config::kTemMPI) {
        std::printf("      Rode:  mpirun -np 4 ./build/mpi_anel\n");
    } else {
        std::printf("      ⚠ MPI NÃO está instalado nesta máquina, e o alvo\n");
        std::printf("        `mpi_anel` não foi criado. O código está em\n");
        std::printf("        mpi_anel.cpp, completo e comentado, mas NÃO FOI\n");
        std::printf("        COMPILADO NEM EXECUTADO aqui — trate-o como\n");
        std::printf("        referência, não como resultado verificado.\n");
        std::printf("        Para habilitar:\n");
        std::printf("          sudo apt install libopenmpi-dev\n");
        std::printf("          cmake -S . -B build && cmake --build build\n");
        std::printf("          mpirun -np 4 ./build/mpi_anel\n");
    }
}

inline void demo_cuda() {
    std::printf("\n── 4.4 CUDA ──\n");

    std::printf("      O terceiro modelo, e o mais diferente: a GPU tem memória\n");
    std::printf("      PRÓPRIA. Nada do host é visível para ela sem cópia\n");
    std::printf("      explícita, e é essa cópia que decide se compensa.\n\n");

    std::printf("      O kernel, e o índice global — o idioma universal:\n");
    std::printf("        __global__ void saxpy(int n, float a,\n");
    std::printf("                              const float* x, float* y) {\n");
    std::printf("            int i = blockIdx.x * blockDim.x + threadIdx.x;\n");
    std::printf("            if (i < n) y[i] = a*x[i] + y[i];   ← a GUARDA\n");
    std::printf("        }\n");
    std::printf("        saxpy<<<blocos, 256>>>(n, 2.5f, x_dev, y_dev);\n\n");

    std::printf("      ⚠ A guarda `if (i < n)` é OBRIGATÓRIA. O número de\n");
    std::printf("        threads é arredondado para cima até um múltiplo do\n");
    std::printf("        tamanho do bloco, então as últimas têm índice além do\n");
    std::printf("        vetor. Sem o `if`, é acesso fora dos limites — e quem\n");
    std::printf("        acusa é o `compute-sanitizer`, não o ASan.\n\n");

    std::printf("      ⚠ Kernels são ASSÍNCRONOS. O lançamento retorna na hora,\n");
    std::printf("        e um erro DENTRO do kernel só aparece na próxima\n");
    std::printf("        chamada sincronizante. Daí o par:\n");
    std::printf("          cudaGetLastError();       // erro de LANÇAMENTO\n");
    std::printf("          cudaDeviceSynchronize();  // erro de EXECUÇÃO\n");
    std::printf("        Ignorar o código de retorno é o erro nº 1 em CUDA: o\n");
    std::printf("        kernel falha em silêncio e o resultado sai zerado.\n\n");

    std::printf("      ⚠ E a MENTIRA COMUM de benchmark de GPU: reportar só o\n");
    std::printf("        tempo do KERNEL. Com as cópias host↔device incluídas, o\n");
    std::printf("        ganho encolhe ou some. SAXPY faz 1 operação para cada 12\n");
    std::printf("        bytes movidos — é o caso clássico em que a GPU NÃO\n");
    std::printf("        compensa, porque o gargalo é o PCIe.\n");
    std::printf("        A GPU compensa quando (a) os dados FICAM nela entre\n");
    std::printf("        vários kernels, ou (b) a intensidade aritmética é alta:\n");
    std::printf("        matmul, convolução, FFT.\n\n");

    std::printf("      No CMake, CUDA é uma LINGUAGEM, não um pacote:\n");
    std::printf("        include(CheckLanguage)\n");
    std::printf("        check_language(CUDA)              ← pergunta sem abortar\n");
    std::printf("        if(CMAKE_CUDA_COMPILER)\n");
    std::printf("            enable_language(CUDA)\n");
    std::printf("            set(CMAKE_CUDA_ARCHITECTURES native)\n");
    std::printf("            add_executable(cuda_saxpy cuda_saxpy.cu)\n");
    std::printf("        endif()\n");
    std::printf("      ⚠ `enable_language(CUDA)` direto, sem checar, ABORTA a\n");
    std::printf("        configuração em qualquer máquina sem nvcc.\n\n");

    if (config::kTemCUDA) {
        std::printf("      Rode:  ./build/cuda_saxpy\n");
    } else {
        std::printf("      ⚠ Não há CUDA Toolkit nesta máquina (há GPU e driver,\n");
        std::printf("        mas `nvcc` não existe). O alvo `cuda_saxpy` não foi\n");
        std::printf("        criado. O código está em cuda_saxpy.cu, completo e\n");
        std::printf("        comentado, mas NÃO FOI COMPILADO NEM EXECUTADO aqui.\n");
        std::printf("        Para habilitar:\n");
        std::printf("          sudo apt install nvidia-cuda-toolkit\n");
        std::printf("          cmake -S . -B build && cmake --build build\n");
        std::printf("          ./build/cuda_saxpy\n");
    }
}

inline void run() {
    demo_comparacao();
    demo_openmp();
    demo_mpi();
    demo_cuda();
}

}  // namespace demo_paralelo
