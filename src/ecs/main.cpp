// =============================================================================
//  src/ecs/main.cpp
//
//  Demo completo do ECS
//  ─────────────────────
//  Demonstra o ECS implementado do zero em ação:
//  • Criação de entidades com combinações diferentes de componentes
//  • Loop de simulação com múltiplos sistemas
//  • Destruição dinâmica de entidades (projéteis com tempo de vida)
//  • Query mostrando apenas entidades com os componentes necessários
// =============================================================================

#include "world.hpp"
#include "components.hpp"
#include "systems.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

using namespace ecs;

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::orange) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════╗\n"
               "║  {:<44}  ║\n"
               "╚══════════════════════════════════════════════╝\n", title);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo 1: conceito básico — entidade é só um ID
// ─────────────────────────────────────────────────────────────────────────────
void demo_conceito() {
    section("1 · Entidade = ID, Componente = dados");

    World world;

    // Cria entidades — são apenas números
    Entity jogador  = world.criar();
    Entity inimigo  = world.criar();
    Entity particula = world.criar();

    fmt::print("  IDs: jogador={}, inimigo={}, particula={}\n",
               jogador, inimigo, particula);

    // Adiciona componentes — cada entidade tem combinação diferente
    world.adicionar<Posicao>(jogador, 0.0f, 0.0f);
    world.adicionar<Velocidade>(jogador, 5.0f, 0.0f);
    world.adicionar<Saude>(jogador, 100, 100);
    world.adicionar<Jogador>(jogador);
    world.adicionar<Renderavel>(jogador, '@');
    world.adicionar<Tag>(jogador, Tag{"jogador"});

    world.adicionar<Posicao>(inimigo, 50.0f, 30.0f);
    world.adicionar<Velocidade>(inimigo, -2.0f, 0.0f);
    world.adicionar<Saude>(inimigo, 50, 50);
    world.adicionar<Inimigo>(inimigo);
    world.adicionar<Dano>(inimigo, 15, 40.0f);
    world.adicionar<Renderavel>(inimigo, 'X');
    world.adicionar<Tag>(inimigo, Tag{"inimigo-1"});

    world.adicionar<Posicao>(particula, 10.0f, 10.0f);
    world.adicionar<Velocidade>(particula, 1.0f, 3.0f);
    world.adicionar<Tempo_de_vida>(particula, 2.0f, 2.0f);
    world.adicionar<Renderavel>(particula, '.');
    world.adicionar<Tag>(particula, Tag{"particula"});

    fmt::print("  entidades criadas: {}\n", world.num_entidades());

    // Consulta componentes diretamente
    auto& pos_j = world.get<Posicao>(jogador);
    fmt::print("  posição do jogador: ({:.0f}, {:.0f})\n", pos_j.x, pos_j.y);

    // tem<T> — verifica presença de componente
    fmt::print("  jogador tem Saude?     {}\n", world.tem<Saude>(jogador));
    fmt::print("  particula tem Saude?   {}\n", world.tem<Saude>(particula));
    fmt::print("  inimigo tem Jogador?   {}\n", world.tem<Jogador>(inimigo));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo 2: query — o poder do ECS
// ─────────────────────────────────────────────────────────────────────────────
void demo_query() {
    section("2 · Query — só entidades com os componentes certos");

    World world;

    // Cria entidades com combinações variadas
    auto e1 = world.criar(); // Posicao + Velocidade
    auto e2 = world.criar(); // Posicao + Velocidade + Saude
    auto e3 = world.criar(); // Posicao (sem velocidade)
    auto e4 = world.criar(); // Velocidade (sem posição — inválido mas demonstrativo)

    world.adicionar<Posicao>(e1, 1.0f, 1.0f);
    world.adicionar<Velocidade>(e1, 1.0f, 0.0f);
    world.adicionar<Tag>(e1, Tag{"e1"});

    world.adicionar<Posicao>(e2, 2.0f, 2.0f);
    world.adicionar<Velocidade>(e2, 0.0f, 1.0f);
    world.adicionar<Saude>(e2);
    world.adicionar<Tag>(e2, Tag{"e2"});

    world.adicionar<Posicao>(e3, 3.0f, 3.0f);
    world.adicionar<Tag>(e3, Tag{"e3"});

    world.adicionar<Velocidade>(e4, 2.0f, 2.0f);
    world.adicionar<Tag>(e4, Tag{"e4"});

    fmt::print("  total de entidades: {}\n", world.num_entidades());

    // Query por Posicao + Velocidade → apenas e1 e e2
    fmt::print("\n  query<Posicao, Velocidade>:\n");
    world.query<Posicao, Velocidade>([&](Entity e, Posicao& p, Velocidade& v) {
        fmt::print("    {} → pos({:.0f},{:.0f}) vel({:.0f},{:.0f})\n",
                   world.get<Tag>(e).nome, p.x, p.y, v.dx, v.dy);
    });

    // Query por Posicao apenas → e1, e2, e3
    fmt::print("\n  query<Posicao>:\n");
    world.query<Posicao>([&](Entity e, Posicao& p) {
        fmt::print("    {} → pos({:.0f},{:.0f})\n",
                   world.get<Tag>(e).nome, p.x, p.y);
    });

    // Query por Posicao + Velocidade + Saude → apenas e2
    fmt::print("\n  query<Posicao, Velocidade, Saude>:\n");
    world.query<Posicao, Velocidade, Saude>([&](Entity e, Posicao&, Velocidade&, Saude& s) {
        fmt::print("    {} → HP {}/{}\n",
                   world.get<Tag>(e).nome, s.hp_atual, s.hp_max);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo 3: loop de simulação — ECS em ação
// ─────────────────────────────────────────────────────────────────────────────
void demo_simulacao() {
    section("3 · Loop de simulação (5 frames)");

    World world;
    const float DT = 0.5f; // delta time por frame

    // ── Cria o jogador ────────────────────────────────────────────────────────
    Entity jogador = world.criar();
    world.adicionar<Posicao>(jogador, 0.0f, 0.0f);
    world.adicionar<Velocidade>(jogador, 10.0f, 0.0f);
    world.adicionar<Saude>(jogador, 100, 100);
    world.adicionar<Jogador>(jogador);
    world.adicionar<Renderavel>(jogador, '@');
    world.adicionar<Colisao>(jogador, 5.0f);
    world.adicionar<Tag>(jogador, Tag{"jogador"});

    // ── Cria inimigos ─────────────────────────────────────────────────────────
    Entity ini1 = world.criar();
    world.adicionar<Posicao>(ini1, 80.0f, 0.0f);
    world.adicionar<Velocidade>(ini1, -8.0f, 0.0f);
    world.adicionar<Saude>(ini1, 30, 30);
    world.adicionar<Inimigo>(ini1);
    world.adicionar<Dano>(ini1, 10, 6.0f);
    world.adicionar<Renderavel>(ini1, 'E');
    world.adicionar<Colisao>(ini1, 5.0f);
    world.adicionar<Tag>(ini1, Tag{"inimigo"});

    // ── Cria projéteis (com tempo de vida) ────────────────────────────────────
    for (int i = 0; i < 3; ++i) {
        Entity proj = world.criar();
        world.adicionar<Posicao>(proj, 0.0f, static_cast<float>(i * 10));
        world.adicionar<Velocidade>(proj, 20.0f, 0.0f);
        world.adicionar<Tempo_de_vida>(proj, 1.5f, 1.5f);
        world.adicionar<Renderavel>(proj, '*');
        world.adicionar<Colisao>(proj, 2.0f);
        world.adicionar<Tag>(proj, Tag{fmt::format("proj-{}", i)});
    }

    fmt::print("  entidades iniciais: {}\n", world.num_entidades());

    // ── Loop principal ────────────────────────────────────────────────────────
    for (int frame = 1; frame <= 5; ++frame) {
        fmt::print("\n  --- frame {} (t={:.1f}s) ---\n",
                   frame, frame * DT);

        // Ordem de execução dos sistemas é controlada aqui
        systems::ia(world);
        systems::aceleracao(world, DT);
        systems::movimento(world, DT);
        systems::colisao(world);
        systems::dano(world);
        systems::tempo_de_vida(world, DT);
        systems::renderizar(world);

        fmt::print("  entidades vivas: {}\n", world.num_entidades());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo 4: composição dinâmica — adicionar/remover componentes em runtime
// ─────────────────────────────────────────────────────────────────────────────
void demo_composicao() {
    section("4 · Composição dinâmica em runtime");

    World world;

    Entity e = world.criar();
    world.adicionar<Tag>(e, Tag{"transformavel"});
    world.adicionar<Posicao>(e, 0.0f, 0.0f);

    fmt::print("  tem Velocidade? {}\n", world.tem<Velocidade>(e));

    // Adiciona velocidade em runtime — comportamento muda sem criar nova entidade
    world.adicionar<Velocidade>(e, 5.0f, 3.0f);
    fmt::print("  após adicionar Velocidade: {}\n", world.tem<Velocidade>(e));

    systems::movimento(world, 1.0f);
    auto& pos = world.get<Posicao>(e);
    fmt::print("  pos após movimento: ({:.0f}, {:.0f})\n", pos.x, pos.y);

    // Remove velocidade — entidade para de se mover
    world.remover<Velocidade>(e);
    fmt::print("  após remover Velocidade: {}\n", world.tem<Velocidade>(e));

    systems::movimento(world, 1.0f); // não move mais
    fmt::print("  pos após remover vel: ({:.0f}, {:.0f}) (não mudou)\n",
               pos.x, pos.y);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo 5: reutilização de IDs — reciclagem de entidades
// ─────────────────────────────────────────────────────────────────────────────
void demo_reciclagem() {
    section("5 · Reciclagem de IDs de entidades");

    World world;

    Entity e1 = world.criar();
    Entity e2 = world.criar();
    Entity e3 = world.criar();
    fmt::print("  criados: {}, {}, {}\n", e1, e2, e3);

    world.destruir(e2);
    fmt::print("  destruído: {}\n", e2);
    fmt::print("  entidades vivas: {}\n", world.num_entidades());

    // Próxima entidade criada reutiliza o ID de e2
    Entity novo = world.criar();
    fmt::print("  novo ID: {} (reutilizou {})\n", novo, e2);
    fmt::print("  e2 ainda existe? {}\n", world.existe(e2));
    fmt::print("  novo existe?     {}\n", world.existe(novo));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    fmt::print(fmt::fg(fmt::color::orange) | fmt::emphasis::bold,
               "\n  ECS — Entity Component System do zero em C++17\n");

    demo_conceito();
    demo_query();
    demo_simulacao();
    demo_composicao();
    demo_reciclagem();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ ECS demo concluído!\n\n");
}
