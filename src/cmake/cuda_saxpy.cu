// =============================================================================
//  src/cmake/cuda_saxpy.cu
//
//  CUDA — paralelismo em GPU
//  ─────────────────────────
//  Um terceiro modelo, e o mais diferente dos três:
//
//    OpenMP → dezenas de threads, memória compartilhada, mesma CPU
//    MPI    → dezenas de processos, memórias separadas, várias máquinas
//    CUDA   → DEZENAS DE MILHARES de threads, num dispositivo com memória
//             PRÓPRIA, que precisa receber e devolver os dados explicitamente
//
//  A extensão `.cu` importa: o CMake a reconhece e chama `nvcc` em vez de
//  `g++`. É por isso que este subprojeto habilita a linguagem CUDA com
//  `check_language(CUDA)` antes de criar o alvo — ver a seção 5 do
//  CMakeLists.txt.
//
//  ⚠ NÃO VERIFICADO NESTA MÁQUINA: há GPU NVIDIA visível (nvidia-smi
//    responde), mas NÃO há CUDA Toolkit — `nvcc` não existe. O CMakeLists.txt
//    detecta isso e pula o alvo. Para habilitar:
//        sudo apt install nvidia-cuda-toolkit     # ou o instalador da NVIDIA
//        cmake -S . -B build && cmake --build build
//        ./build/cuda_saxpy
// =============================================================================
#include "nucleo.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <cuda_runtime.h>

// ── Checagem de erro ────────────────────────────────────────────────────────
//  ⚠ TODA chamada de CUDA devolve um código, e ignorá-lo é o erro nº 1 de
//    quem começa: o kernel falha em silêncio e o resultado sai zerado.
//    Pior: kernels são ASSÍNCRONOS, então o erro de um kernel só aparece na
//    PRÓXIMA chamada sincronizante. Daí o par de checagens em `checa_kernel`.
#define CUDA_OK(chamada)                                                      \
    do {                                                                      \
        const cudaError_t erro_ = (chamada);                                  \
        if (erro_ != cudaSuccess) {                                           \
            std::fprintf(stderr, "CUDA erro em %s:%d — %s\n", __FILE__,       \
                         __LINE__, cudaGetErrorString(erro_));                \
            std::exit(1);                                                     \
        }                                                                     \
    } while (0)

static void checa_kernel(const char* nome) {
    CUDA_OK(cudaGetLastError());        // erro de LANÇAMENTO (config inválida)
    CUDA_OK(cudaDeviceSynchronize());   // erro de EXECUÇÃO (acesso inválido)
    (void)nome;
}

// ── O kernel ────────────────────────────────────────────────────────────────
//  `__global__` = roda na GPU, chamado da CPU.
//  Cada thread calcula UM elemento. O índice global vem da posição do bloco
//  mais a posição dentro do bloco — é o idioma universal de CUDA.
__global__ void saxpy_kernel(int n, float a, const float* x, float* y) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;

    // ⚠ A guarda é OBRIGATÓRIA. O número de threads lançadas é arredondado
    //   para cima até um múltiplo do tamanho do bloco, então as últimas
    //   threads têm índice além do vetor. Sem o `if`, é acesso fora dos
    //   limites — em GPU, e o compute-sanitizer é quem acusa.
    if (i < n) y[i] = a * x[i] + y[i];
}

int main(int argc, char** argv) {
    const int n = argc > 1 ? std::atoi(argv[1]) : 50'000'000;

    // ── Qual GPU? ───────────────────────────────────────────────────────────
    int dispositivos = 0;
    CUDA_OK(cudaGetDeviceCount(&dispositivos));
    if (dispositivos == 0) {
        std::printf("Nenhuma GPU CUDA encontrada.\n");
        return 1;
    }
    cudaDeviceProp prop{};
    CUDA_OK(cudaGetDeviceProperties(&prop, 0));
    std::printf("GPU: %s (compute %d.%d, %d SMs, %.1f GB)\n", prop.name,
                prop.major, prop.minor, prop.multiProcessorCount,
                static_cast<double>(prop.totalGlobalMem) / (1024.0 * 1024 * 1024));

    // ── Dados no HOST ───────────────────────────────────────────────────────
    const auto x_host = nucleo::gerar(static_cast<std::size_t>(n), 1);
    auto y_ref        = nucleo::gerar(static_cast<std::size_t>(n), 2);
    auto y_host       = y_ref;

    const auto t_cpu0 = std::chrono::steady_clock::now();
    nucleo::saxpy(2.5f, x_host, y_ref);
    const double t_cpu =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t_cpu0).count();

    // ── Dados no DEVICE ─────────────────────────────────────────────────────
    //  A GPU tem memória PRÓPRIA. Nada do host é visível para ela sem cópia.
    float *x_dev = nullptr, *y_dev = nullptr;
    const std::size_t bytes = static_cast<std::size_t>(n) * sizeof(float);

    CUDA_OK(cudaMalloc(&x_dev, bytes));
    CUDA_OK(cudaMalloc(&y_dev, bytes));

    // Cronometramos com eventos CUDA, que medem no relógio da GPU.
    cudaEvent_t inicio, fim;
    CUDA_OK(cudaEventCreate(&inicio));
    CUDA_OK(cudaEventCreate(&fim));

    const auto t_total0 = std::chrono::steady_clock::now();

    CUDA_OK(cudaMemcpy(x_dev, x_host.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_OK(cudaMemcpy(y_dev, y_host.data(), bytes, cudaMemcpyHostToDevice));

    // ── Lançamento ──────────────────────────────────────────────────────────
    //  256 threads por bloco é o valor de partida convencional: múltiplo de 32
    //  (o tamanho do warp) e pequeno o bastante para caber nos registradores.
    constexpr int kThreadsPorBloco = 256;
    const int kBlocos = (n + kThreadsPorBloco - 1) / kThreadsPorBloco;

    CUDA_OK(cudaEventRecord(inicio));
    saxpy_kernel<<<kBlocos, kThreadsPorBloco>>>(n, 2.5f, x_dev, y_dev);
    CUDA_OK(cudaEventRecord(fim));
    checa_kernel("saxpy_kernel");

    float t_kernel_ms = 0.0f;
    CUDA_OK(cudaEventElapsedTime(&t_kernel_ms, inicio, fim));

    CUDA_OK(cudaMemcpy(y_host.data(), y_dev, bytes, cudaMemcpyDeviceToHost));

    const double t_total =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t_total0).count();

    // ── O que interessa ─────────────────────────────────────────────────────
    const double erro = nucleo::erro_maximo(y_ref, y_host);

    std::printf("\nSAXPY sobre %d elementos:\n", n);
    std::printf("  CPU serial            : %8.2f ms\n", t_cpu);
    std::printf("  GPU, SÓ o kernel      : %8.2f ms   (%.1fx)\n",
                t_kernel_ms, t_cpu / t_kernel_ms);
    std::printf("  GPU, COM as cópias    : %8.2f ms   (%.1fx)\n",
                t_total, t_cpu / t_total);
    std::printf("  erro máximo           : %.3e\n", erro);
    std::printf("  banda efetiva         : %.1f GB/s\n",
                3.0 * bytes / (t_kernel_ms / 1000.0) / 1e9);
    std::printf("  %d blocos × %d threads = %d threads lançadas\n",
                kBlocos, kThreadsPorBloco, kBlocos * kThreadsPorBloco);

    std::printf("\n⚠ Compare as DUAS linhas de GPU. O kernel é muito mais\n");
    std::printf("  rápido que a CPU; COM as cópias, o ganho encolhe ou some.\n");
    std::printf("  SAXPY faz 1 operação para cada 12 bytes movidos — é o caso\n");
    std::printf("  clássico em que a GPU NÃO compensa, porque o gargalo é o\n");
    std::printf("  PCIe, não o cálculo.\n");
    std::printf("  A GPU compensa quando (a) os dados FICAM nela entre vários\n");
    std::printf("  kernels, ou (b) a intensidade aritmética é alta (matmul,\n");
    std::printf("  convolução, FFT). Medir só o kernel é a forma mais comum de\n");
    std::printf("  mentir com benchmark de GPU.\n");

    CUDA_OK(cudaEventDestroy(inicio));
    CUDA_OK(cudaEventDestroy(fim));
    CUDA_OK(cudaFree(x_dev));
    CUDA_OK(cudaFree(y_dev));
    return 0;
}
