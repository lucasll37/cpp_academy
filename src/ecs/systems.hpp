// =============================================================================
//  src/ecs/systems.hpp
//
//  Systems — lógica pura, zero dados próprios
//  ────────────────────────────────────────────
//  Cada system é uma função (ou objeto callable) que:
//  • Recebe o World por referência
//  • Faz query dos componentes que precisa
//  • Atualiza o estado do mundo
//
//  Systems são completamente independentes entre si.
//  A ordem de execução é controlada pelo loop principal (main.cpp).
//
//  Regra: um system não deve guardar estado próprio.
//  Se precisar de estado persistente, crie um componente "singleton"
//  (uma entidade especial que representa o mundo/configuração).
// =============================================================================
#pragma once
#include "world.hpp"
#include "components.hpp"
#include <fmt/core.h>
#include <cmath>

namespace ecs::systems {

// ─────────────────────────────────────────────────────────────────────────────
//  Sistema de Movimento
//  Entidades necessárias: Posicao + Velocidade
//  Aplica velocidade à posição a cada frame
// ─────────────────────────────────────────────────────────────────────────────
void movimento(World& world, float dt) {
    world.query<Posicao, Velocidade>([&](Entity, Posicao& pos, Velocidade& vel) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sistema de Aceleração
//  Entidades: Velocidade + Aceleracao
//  Aplica aceleração à velocidade (física simples)
// ─────────────────────────────────────────────────────────────────────────────
void aceleracao(World& world, float dt) {
    world.query<Velocidade, Aceleracao>([&](Entity, Velocidade& vel, Aceleracao& ac) {
        vel.dx += ac.ax * dt;
        vel.dy += ac.ay * dt;
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sistema de Tempo de Vida
//  Entidades: Tempo_de_vida
//  Diminui o contador e marca entidades para destruição
// ─────────────────────────────────────────────────────────────────────────────
void tempo_de_vida(World& world, float dt) {
    // Coleta entidades expiradas primeiro (não destruir durante iteração)
    std::vector<Entity> expiradas;

    world.query<Tempo_de_vida>([&](Entity e, Tempo_de_vida& tdv) {
        tdv.restante -= dt;
        if (tdv.restante <= 0.0f)
            expiradas.push_back(e);
    });

    for (Entity e : expiradas) {
        std::string nome = world.tem<Tag>(e) ? world.get<Tag>(e).nome : "?";
        fmt::print("    [ttl] entidade '{}' expirou\n", nome);
        world.destruir(e);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sistema de Colisão (simplificado)
//  Entidades: Posicao + Colisao
//  Detecta pares de entidades que se sobrepõem (O(n²) — apenas didático)
// ─────────────────────────────────────────────────────────────────────────────
void colisao(World& world) {
    // Coleta todas as entidades com Posicao + Colisao
    struct Candidato { Entity e; float x, y, raio; };
    std::vector<Candidato> candidatos;

    world.query<Posicao, Colisao>([&](Entity e, Posicao& pos, Colisao& col) {
        col.colidindo = false; // reset
        candidatos.push_back({e, pos.x, pos.y, col.raio});
    });

    // Testa todos os pares
    for (std::size_t i = 0; i < candidatos.size(); ++i) {
        for (std::size_t j = i + 1; j < candidatos.size(); ++j) {
            auto& a = candidatos[i];
            auto& b = candidatos[j];

            float dx   = a.x - b.x;
            float dy   = a.y - b.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            float soma_raios = a.raio + b.raio;

            if (dist < soma_raios) {
                world.get<Colisao>(a.e).colidindo = true;
                world.get<Colisao>(b.e).colidindo = true;

                std::string na = world.tem<Tag>(a.e) ? world.get<Tag>(a.e).nome : "?";
                std::string nb = world.tem<Tag>(b.e) ? world.get<Tag>(b.e).nome : "?";
                fmt::print("    [colisao] '{}' ↔ '{}' (dist={:.1f})\n", na, nb, dist);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sistema de Dano
//  Entidades atacantes: Posicao + Dano + Inimigo
//  Entidades alvo:      Posicao + Saude + Jogador
//  Se inimigo alertado e dentro do alcance → aplica dano
// ─────────────────────────────────────────────────────────────────────────────
void dano(World& world) {
    world.query<Posicao, Dano, Inimigo>(
        [&](Entity ei, Posicao& pi, Dano& d, Inimigo& ini) {
            if (!ini.alertado) return;

            world.query<Posicao, Saude, Jogador>(
                [&](Entity ej, Posicao& pj, Saude& s, Jogador&) {
                    float dx = pi.x - pj.x;
                    float dy = pi.y - pj.y;
                    float dist = std::sqrt(dx*dx + dy*dy);

                    if (dist <= d.alcance && s.vivo()) {
                        s.hp_atual -= d.valor;
                        fmt::print("    [dano] inimigo atacou jogador! HP: {}/{}\n",
                                   s.hp_atual, s.hp_max);
                    }
                    (void)ei; (void)ej;
                }
            );
        }
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sistema de Renderização (ASCII)
//  Entidades: Posicao + Renderavel
//  Imprime o estado visual simplificado do mundo
// ─────────────────────────────────────────────────────────────────────────────
void renderizar(World& world) {
    fmt::print("    [render] ");
    world.query<Posicao, Renderavel>(
        [&](Entity e, Posicao& pos, Renderavel& r) {
            if (!r.visivel) return;
            std::string nome = world.tem<Tag>(e) ? world.get<Tag>(e).nome : "?";
            fmt::print("{}@({:.0f},{:.0f}) ", r.simbolo, pos.x, pos.y);
            (void)nome;
        }
    );
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sistema de IA simples
//  Entidades: Posicao + Inimigo
//  Alerta inimigos se jogador estiver perto
// ─────────────────────────────────────────────────────────────────────────────
void ia(World& world) {
    constexpr float RAIO_DETECCAO = 80.0f;

    // Posição do jogador
    float jx = 0, jy = 0;
    bool jogador_encontrado = false;
    world.query<Posicao, Jogador>([&](Entity, Posicao& p, Jogador&) {
        jx = p.x; jy = p.y;
        jogador_encontrado = true;
    });
    if (!jogador_encontrado) return;

    world.query<Posicao, Inimigo>([&](Entity, Posicao& pi, Inimigo& ini) {
        float dx = pi.x - jx;
        float dy = pi.y - jy;
        float dist = std::sqrt(dx*dx + dy*dy);
        bool antes = ini.alertado;
        ini.alertado = dist <= RAIO_DETECCAO;
        if (ini.alertado && !antes)
            fmt::print("    [ia] inimigo alertado! (dist={:.1f})\n", dist);
    });
}

} // namespace ecs::systems
