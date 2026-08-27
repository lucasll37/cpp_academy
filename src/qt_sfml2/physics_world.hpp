#pragma once

// ============================================================
// PhysicsWorld
//
// Encapsula o pipeline completo do Bullet3:
//   - btDiscreteDynamicsWorld  : mundo de física
//   - btDbvtBroadphase         : fase larga (AABB tree)
//   - btSequentialImpulseConstraintSolver
//   - btDefaultCollisionConfiguration + btCollisionDispatcher
//
// Cada objeto simulado é representado por um PhysicsObject,
// que agrupa o btRigidBody com metadados de renderização
// (cor, tipo de forma) para o SFML desenhar.
//
// Arquitetura de responsabilidades:
//   PhysicsWorld  → apenas física (sem SFML, sem Qt)
//   SFMLWidget    → apenas renderização (projeta 3D → 2D)
//   MainWindow    → apenas UI (sliders, botões, menus)
// ============================================================

#include <btBulletDynamicsCommon.h>
#include <memory>
#include <vector>
#include <functional>
#include <SFML/Graphics/Color.hpp>

// ─── Tipos de forma suportados ────────────────────────────────────────────────
enum class ShapeType { Box, Sphere, Cylinder };

// ─── Representação de um objeto físico ───────────────────────────────────────
struct PhysicsObject {
    std::unique_ptr<btCollisionShape>  shape;
    std::unique_ptr<btMotionState>     motion_state;
    std::unique_ptr<btRigidBody>       body;

    ShapeType   type;
    sf::Color   color;
    float       half_extent;  // raio ou metade do lado
};

class PhysicsWorld {
public:
    // half_room: metade das dimensões do quarto cúbico (ex: 5 → quarto 10×10×10)
    explicit PhysicsWorld(float half_room = 5.f);
    ~PhysicsWorld() = default;

    // Avança a simulação por `dt` segundos
    void step(float dt);

    // Adiciona um objeto com posição, velocidade inicial e impulso aleatório
    void add_object(ShapeType type, float half_extent,
                    const btVector3& position,
                    const btVector3& initial_velocity,
                    sf::Color color);

    // Remove todos os objetos (mantém as paredes)
    void clear_objects();

    // Aplica impulso radial a todos os objetos (efeito "explosão")
    void explode(float force);

    // Altera a gravidade (Y negativo = para baixo)
    void set_gravity(float gy);

    // Acesso de leitura aos objetos para a camada de renderização
    const std::vector<std::unique_ptr<PhysicsObject>>& objects() const {
        return objects_;
    }

    float half_room() const { return half_room_; }

private:
    // Cria os 6 planos estáticos que formam as paredes do quarto
    void build_room();

    // Helper para criar um plano infinito estático
    void add_static_plane(const btVector3& normal, float constant);

    float half_room_;

    // ── Pipeline Bullet3 ─────────────────────────────────────────────────────
    // A ordem de destruição importa: o mundo deve ser destruído antes das
    // configurações. unique_ptr destrói na ordem inversa de declaração —
    // por isso dynamics_world_ vem antes na lista (destruído depois dos outros).
    std::unique_ptr<btDefaultCollisionConfiguration>     config_;
    std::unique_ptr<btCollisionDispatcher>               dispatcher_;
    std::unique_ptr<btDbvtBroadphase>                    broadphase_;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;
    std::unique_ptr<btDiscreteDynamicsWorld>             world_;

    // Paredes estáticas (planos)
    std::vector<std::unique_ptr<btCollisionShape>>  wall_shapes_;
    std::vector<std::unique_ptr<btMotionState>>     wall_states_;
    std::vector<std::unique_ptr<btRigidBody>>       wall_bodies_;

    // Objetos dinâmicos
    std::vector<std::unique_ptr<PhysicsObject>>     objects_;
};
