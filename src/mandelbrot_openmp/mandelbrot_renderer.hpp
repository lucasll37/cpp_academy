#pragma once

// ============================================================
// MandelbrotRenderer — versão OpenMP
//
// Paralelismo via #pragma omp parallel for schedule(dynamic).
// Cada iteração do loop externo (linha y) é uma unidade de
// trabalho. O scheduler dinâmico redistribui chunks de linhas
// entre as threads em tempo de execução, balanceando o custo
// variável por linha (borda do conjunto = mais iterações).
//
// Comparação com a versão std::thread:
//   std::thread  → explícito: divide tiles, lança threads,
//                  mutex + condition_variable, join manual.
//   OpenMP       → implícito: pragma anota o loop, runtime
//                  gerencia threads, barreira implícita no fim.
// ============================================================

#include <cstdint>
#include <array>
#include <vector>

struct Viewport {
    double cx    = -0.5;
    double cy    =  0.0;
    double scale =  3.0;
};

class MandelbrotRenderer {
public:
    MandelbrotRenderer(int w, int h, int num_threads, int max_iter);

    // Renderiza de forma síncrona. Internamente usa omp parallel for.
    // Retorna referência para o buffer RGBA interno (w*h*4 bytes).
    const std::vector<uint8_t>& render(const Viewport& vp);

    void resize(int w, int h);
    void set_threads(int n);
    void set_max_iter(int n);

    int width()    const { return w_; }
    int height()   const { return h_; }
    int threads()  const { return num_threads_; }
    int max_iter() const { return max_iter_; }

private:
    static std::array<uint8_t,4> colorize(int iter, int max_iter);

    int w_, h_;
    int num_threads_;
    int max_iter_;

    std::vector<uint8_t> pixels_;   // buffer RGBA — cada thread escreve
                                    // em offsets disjuntos → sem corrida
};
