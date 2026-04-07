// =============================================================================
//  src/ecs/components.hpp
//
//  Componentes — dados puros, zero lógica
//  ─────────────────────────────────────────
//  Cada componente é uma struct simples com dados relacionados.
//  Sem herança, sem métodos virtuais, sem lógica de negócio.
//
//  Regra: se você está colocando um método com lógica em um componente,
//  provavelmente essa lógica pertence a um System.
// =============================================================================
#pragma once
#include <string>
#include <cstdint>

namespace ecs {

// ── Física / posicionamento ───────────────────────────────────────────────────

struct Posicao {
    float x = 0.0f;
    float y = 0.0f;
    Posicao() = default;
    Posicao(float x, float y) : x(x), y(y) {}
};

struct Velocidade {
    float dx = 0.0f;
    float dy = 0.0f;
    Velocidade() = default;
    Velocidade(float dx, float dy) : dx(dx), dy(dy) {}
};

struct Aceleracao {
    float ax = 0.0f;
    float ay = 0.0f;
    Aceleracao() = default;
    Aceleracao(float ax, float ay) : ax(ax), ay(ay) {}
};

struct Colisao {
    float raio = 1.0f;
    bool  colidindo = false;
    Colisao() = default;
    explicit Colisao(float raio) : raio(raio) {}
};

// ── Gameplay ──────────────────────────────────────────────────────────────────

struct Saude {
    int hp_atual  = 100;
    int hp_max    = 100;
    Saude() = default;
    Saude(int atual, int max) : hp_atual(atual), hp_max(max) {}
    bool vivo()  const { return hp_atual > 0; }
    float ratio() const { return static_cast<float>(hp_atual) / hp_max; }
};

struct Dano {
    int   valor   = 10;
    float alcance = 50.0f;
    Dano() = default;
    Dano(int valor, float alcance) : valor(valor), alcance(alcance) {}
};

struct Tempo_de_vida {
    float restante = 5.0f;
    float total    = 5.0f;
    Tempo_de_vida() = default;
    Tempo_de_vida(float restante, float total) : restante(restante), total(total) {}
};

// ── Renderização ──────────────────────────────────────────────────────────────

struct Renderavel {
    char  simbolo = '?';
    float escala  = 1.0f;
    bool  visivel = true;
    Renderavel() = default;
    explicit Renderavel(char simbolo) : simbolo(simbolo) {}
    Renderavel(char simbolo, float escala) : simbolo(simbolo), escala(escala) {}
};

struct Cor {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    Cor() = default;
    Cor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}
};

// ── Identificação / metadados ─────────────────────────────────────────────────

struct Tag {
    std::string nome;
    Tag() = default;
    explicit Tag(std::string nome) : nome(std::move(nome)) {}
};

struct Jogador {
    int score = 0;
    int nivel = 1;
    Jogador() = default;
};

struct Inimigo {
    float agressividade = 1.0f;
    bool  alertado      = false;
    Inimigo() = default;
};

} // namespace ecs
