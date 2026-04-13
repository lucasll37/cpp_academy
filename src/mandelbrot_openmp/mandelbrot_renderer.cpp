#include "mandelbrot_renderer.hpp"

#include <omp.h>
#include <cmath>
#include <algorithm>

// ============================================================
// Paleta HSV → RGBA  (mesma lógica da versão anterior)
// ============================================================
static std::array<uint8_t,4> hsv_to_rgba(float h, float s, float v) {
    float c  = v * s;
    float x  = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m  = v - c;
    float r1, g1, b1;
    int   hi = (int)(h / 60.0f) % 6;
    if      (hi==0){ r1=c; g1=x; b1=0; }
    else if (hi==1){ r1=x; g1=c; b1=0; }
    else if (hi==2){ r1=0; g1=c; b1=x; }
    else if (hi==3){ r1=0; g1=x; b1=c; }
    else if (hi==4){ r1=x; g1=0; b1=c; }
    else           { r1=c; g1=0; b1=x; }
    return {
        (uint8_t)((r1+m)*255),
        (uint8_t)((g1+m)*255),
        (uint8_t)((b1+m)*255),
        255
    };
}

std::array<uint8_t,4> MandelbrotRenderer::colorize(int iter, int max_iter) {
    if (iter == max_iter) return {0, 0, 0, 255};
    float t   = (float)iter / (float)max_iter;
    float hue = std::fmod(240.0f - t * 200.0f + 360.0f, 360.0f);
    float val = t < 0.01f ? t * 100.0f : 1.0f;
    return hsv_to_rgba(hue, 0.85f, val);
}

// ------------------------------------------------------------
MandelbrotRenderer::MandelbrotRenderer(int w, int h, int num_threads, int max_iter)
    : w_(w), h_(h), num_threads_(num_threads), max_iter_(max_iter)
{
    pixels_.resize(w_ * h_ * 4, 0);
}

void MandelbrotRenderer::resize(int w, int h) {
    w_ = w; h_ = h;
    pixels_.resize(w_ * h_ * 4, 0);
}

void MandelbrotRenderer::set_threads(int n) { num_threads_ = std::max(1, n); }
void MandelbrotRenderer::set_max_iter(int n) { max_iter_   = std::max(16, n); }

// ------------------------------------------------------------
// render() — versão OpenMP
//
// #pragma omp parallel for
//   Divide as iterações do loop entre num_threads_ threads.
//   Cada thread executa um subconjunto de valores de py.
//
// schedule(dynamic, 4)
//   Chunk de 4 linhas por vez. Quando uma thread termina seu
//   chunk ela pega o próximo disponível. Isso balanceia o custo
//   irregular entre linhas: borda do conjunto = caro,
//   interior/exterior = barato.
//   schedule(static) seria mais rápido se o custo fosse uniforme,
//   mas aqui o dynamic compensa claramente.
//
// num_threads(num_threads_)
//   Respeita o slider da UI. omp_set_num_threads() também
//   funcionaria mas num_threads() é mais localizado.
//
// Sem mutex: cada py mapeia para um offset único em pixels_
//   (py * w_ * 4), portanto threads escrevem em regiões
//   completamente disjuntas — zero contenção, zero corrida.
// ------------------------------------------------------------
const std::vector<uint8_t>& MandelbrotRenderer::render(const Viewport& vp) {
    const double aspect = (double)w_ / (double)h_;

    #pragma omp parallel for schedule(dynamic, 4) num_threads(num_threads_)
    for (int py = 0; py < h_; ++py) {
        const double ci = vp.cy + (py / (double)(h_ - 1) - 0.5) * vp.scale / aspect;

        for (int px = 0; px < w_; ++px) {
            const double cr = vp.cx + (px / (double)(w_ - 1) - 0.5) * vp.scale;

            // z_{n+1} = z_n² + c
            double zr = 0.0, zi = 0.0;
            int iter = 0;
            while (iter < max_iter_ && zr*zr + zi*zi < 4.0) {
                double zr2 = zr*zr - zi*zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = zr2;
                ++iter;
            }

            // Escrita em offset exclusivo desta thread — sem mutex necessário
            const int off = (py * w_ + px) * 4;
            const auto col = colorize(iter, max_iter_);
            pixels_[off+0] = col[0];
            pixels_[off+1] = col[1];
            pixels_[off+2] = col[2];
            pixels_[off+3] = col[3];
        }
    }

    return pixels_;
}
