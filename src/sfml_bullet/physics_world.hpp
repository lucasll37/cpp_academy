#pragma once
#include <btBulletDynamicsCommon.h>
#include <vector>
#include <memory>

// ============================================================
// PhysicsWorld
//
// Wrapper sobre o pipeline completo do Bullet3:
//   btDefaultCollisionConfiguration
//   btCollisionDispatcher      → narrowphase
//   btDbvtBroadphase           → broadphase (AABB tree dinâmica)
//   btSequentialImpulseConstraintSolver
//   btDiscreteDynamicsWorld    → o mundo que une tudo
//
// Mantém ownership de todos os rigid bodies e shapes via
// vetores de unique_ptr — o Bullet não faz gerenciamento.
//
// Coordenadas: usamos o plano XY do Bullet como tela 2D.
//   X → direita, Y → cima, Z = 0 (ignoramos Z).
// ============================================================

namespace sim {

// Tipos de shape suportados
enum class ShapeType { Sphere, Box };

// Dados de criação de um corpo
struct BodyDef {
    ShapeType type;
    float     x, y;           // posição inicial (pixels convertidos)
    float     half_extent;    // raio ou meia-largura da caixa
    float     mass;           // 0 = estático
    float     restitution;    // elasticidade [0, 1]
    float     friction;
};

// Handle leve para consultar estado de um corpo
struct BodyHandle {
    btRigidBody*        body;
    btCollisionShape*   shape;
    ShapeType           type;
    float               half_extent;
};

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    // unique_ptr members deletam copy — declaramos move explicitamente
    PhysicsWorld(PhysicsWorld&&)            = default;
    PhysicsWorld& operator=(PhysicsWorld&&) = default;

    // Avança a simulação por `dt` segundos
    void step(float dt);

    // Cria um corpo e devolve handle para leitura de estado
    BodyHandle add_body(const BodyDef& def);

    // Acessa todos os corpos dinâmicos (para sincronizar com SFML)
    const std::vector<BodyHandle>& bodies() const { return bodies_; }

private:
    // Ordem de criação importa — destruição é na ordem inversa
    std::unique_ptr<btDefaultCollisionConfiguration>       config_;
    std::unique_ptr<btCollisionDispatcher>                 dispatcher_;
    std::unique_ptr<btDbvtBroadphase>                      broadphase_;
    std::unique_ptr<btSequentialImpulseConstraintSolver>   solver_;
    std::unique_ptr<btDiscreteDynamicsWorld>               world_;

    // Ownership dos corpos e shapes
    std::vector<std::unique_ptr<btRigidBody>>             rigid_bodies_;
    std::vector<std::unique_ptr<btDefaultMotionState>>    motion_states_;
    std::vector<std::unique_ptr<btCollisionShape>>        shapes_;

    // Handles leves para leitura rápida de estado
    std::vector<BodyHandle> bodies_;
};

} // namespace sim