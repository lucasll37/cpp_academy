#pragma once

// ============================================================
// MandelbrotRenderer
//
// Divide o frame em `num_threads` faixas horizontais (tiles).
// Cada tile é entregue a uma std::thread de um pool simples.
//
// Sincronização:
//   - tile_mutex_  protege o vetor pixels_ durante escrita
//     (cada thread escreve em sua própria faixa → contenção
//      zero em condições normais, mas o mutex está lá para
//      demonstrar o padrão correto de proteção)
//   - done_cv_ / done_count_ sinalizam ao caller quando
//     todos os tiles terminaram (padrão produtor/consumidor)
// ============================================================

#include <complex>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>
#include <functional>

struct Viewport {
    double cx = -0.5;   // centro real
    double cy =  0.0;   // centro imaginário
    double scale = 3.0; // largura do plano complexo visível
};

class MandelbrotRenderer {
public:
    MandelbrotRenderer(int w, int h, int num_threads, int max_iter);

    // Renderiza de forma síncrona (bloqueia até todos os tiles terminarem).
    // Retorna ponteiro para buffer RGBA (tamanho = w*h*4).
    const std::vector<uint8_t>& render(const Viewport& vp);

    void resize(int w, int h);
    void set_threads(int n);
    void set_max_iter(int n);

    int width()    const { return w_; }
    int height()   const { return h_; }
    int threads()  const { return num_threads_; }
    int max_iter() const { return max_iter_; }

private:
    // Computa um tile [y0, y1) do frame.
    void compute_tile(int y0, int y1, const Viewport& vp);

    // Mapeia número de iterações → cor RGBA (paleta HSV suave)
    static std::array<uint8_t,4> colorize(int iter, int max_iter);

    int w_, h_;
    int num_threads_;
    int max_iter_;

    std::vector<uint8_t> pixels_;   // buffer RGBA

    // --- sincronização ---
    std::mutex              tile_mutex_;     // protege pixels_ durante escrita paralela
    std::mutex              done_mutex_;     // protege done_count_
    std::condition_variable done_cv_;
    int                     done_count_ = 0;
};
