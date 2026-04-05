#include "ragdoll.hpp"
#include <BulletDynamics/ConstraintSolver/btHingeConstraint.h>
#include <BulletDynamics/ConstraintSolver/btConeTwistConstraint.h>
#include <cmath>

namespace sim {

// Helpers locais
static btTransform make_t(float x, float y, float z = 0.f) {
    btTransform t; t.setIdentity();
    t.setOrigin(btVector3(x, y, z));
    return t;
}

// Cria uma cápsula (cilindro com esferas nas pontas) — shape mais
// adequada para membros do que uma caixa: colisão mais suave,
// melhor comportamento com constraints de ombro/quadril.
static btCapsuleShape* make_capsule(float radius, float height,
                                    PhysicsWorld& pw) {
    auto s = std::make_unique<btCapsuleShape>(radius, height);
    btCapsuleShape* ptr = s.get();
    pw.take_shape(std::move(s));
    return ptr;
}

static btSphereShape* make_sphere(float radius, PhysicsWorld& pw) {
    auto s = std::make_unique<btSphereShape>(radius);
    btSphereShape* ptr = s.get();
    pw.take_shape(std::move(s));
    return ptr;
}

void Ragdoll::build(PhysicsWorld& pw, float cx, float cy) {
    // ── Geometria (em metros) ────────────────────────────────
    //   torso: cápsula alta e larga
    //   cabeça: esfera
    //   braços: cápsulas horizontais
    //   pernas: cápsulas verticais

    // TORSO — capsula vertical (raio 0.2, altura 0.6)
    bodies[TORSO] = pw.add_rigid(
        make_capsule(0.20f, 0.60f, pw),
        5.0f,
        make_t(cx, cy),
        0.1f, 0.8f
    );

    // CABEÇA — esfera (raio 0.18)
    bodies[HEAD] = pw.add_rigid(
        make_sphere(0.18f, pw),
        1.5f,
        make_t(cx, cy + 0.62f),
        0.1f, 0.5f
    );

    // BRAÇO ESQUERDO — cápsula horizontal (raio 0.08, comprimento 0.4)
    bodies[ARM_L] = pw.add_rigid(
        make_capsule(0.08f, 0.35f, pw),
        0.8f,
        make_t(cx - 0.42f, cy + 0.18f),
        0.1f, 0.5f
    );
    // Para o braço ficar horizontal: capsule por padrão é vertical,
    // então rotacionamos 90° em torno de Z
    {
        btTransform t = make_t(cx - 0.42f, cy + 0.18f);
        btQuaternion q; q.setEulerZYX(0.f, 0.f, static_cast<float>(M_PI_2));
        t.setRotation(q);
        bodies[ARM_L]->setCenterOfMassTransform(t);
    }

    // BRAÇO DIREITO
    bodies[ARM_R] = pw.add_rigid(
        make_capsule(0.08f, 0.35f, pw),
        0.8f,
        make_t(cx + 0.42f, cy + 0.18f),
        0.1f, 0.5f
    );
    {
        btTransform t = make_t(cx + 0.42f, cy + 0.18f);
        btQuaternion q; q.setEulerZYX(0.f, 0.f, static_cast<float>(M_PI_2));
        t.setRotation(q);
        bodies[ARM_R]->setCenterOfMassTransform(t);
    }

    // PERNA ESQUERDA — cápsula vertical
    bodies[LEG_L] = pw.add_rigid(
        make_capsule(0.09f, 0.45f, pw),
        1.5f,
        make_t(cx - 0.14f, cy - 0.70f),
        0.1f, 0.8f
    );

    // PERNA DIREITA
    bodies[LEG_R] = pw.add_rigid(
        make_capsule(0.09f, 0.45f, pw),
        1.5f,
        make_t(cx + 0.14f, cy - 0.70f),
        0.1f, 0.8f
    );

    // ── Constraints ──────────────────────────────────────────
    auto* world = pw.world();

    // ---- Pescoço: btConeTwistConstraint ─────────────────────
    //
    // Frames locais: no torso, o pivot fica no topo (y=+0.45);
    // na cabeça, o pivot fica na base (y=-0.18).
    // swingSpan define o ângulo máximo de inclinação lateral/frontal.
    // twistSpan define a rotação ao redor do eixo vertical.
    {
        btTransform frame_torso; frame_torso.setIdentity();
        frame_torso.setOrigin(btVector3(0.f, 0.45f, 0.f));

        btTransform frame_head; frame_head.setIdentity();
        frame_head.setOrigin(btVector3(0.f, -0.18f, 0.f));

        auto* c = new btConeTwistConstraint(
            *bodies[TORSO], *bodies[HEAD],
            frame_torso, frame_head
        );
        c->setLimit(
            static_cast<float>(M_PI) * 0.15f,  // swingSpan1 (frontal)
            static_cast<float>(M_PI) * 0.15f,  // swingSpan2 (lateral)
            static_cast<float>(M_PI) * 0.10f   // twistSpan  (rotação)
        );
        c->setDamping(0.3f);
        world->addConstraint(c, true); // true = desativa colisão entre os dois bodies
    }

    // ---- Ombro esquerdo: btConeTwistConstraint ──────────────
    {
        btTransform frame_torso; frame_torso.setIdentity();
        frame_torso.setOrigin(btVector3(-0.25f, 0.20f, 0.f));

        btTransform frame_arm; frame_arm.setIdentity();
        frame_arm.setOrigin(btVector3(0.25f, 0.f, 0.f));

        auto* c = new btConeTwistConstraint(
            *bodies[TORSO], *bodies[ARM_L],
            frame_torso, frame_arm
        );
        c->setLimit(
            static_cast<float>(M_PI) * 0.5f,
            static_cast<float>(M_PI) * 0.5f,
            static_cast<float>(M_PI) * 0.4f
        );
        world->addConstraint(c, true);
    }

    // ---- Ombro direito ──────────────────────────────────────
    {
        btTransform frame_torso; frame_torso.setIdentity();
        frame_torso.setOrigin(btVector3(0.25f, 0.20f, 0.f));

        btTransform frame_arm; frame_arm.setIdentity();
        frame_arm.setOrigin(btVector3(-0.25f, 0.f, 0.f));

        auto* c = new btConeTwistConstraint(
            *bodies[TORSO], *bodies[ARM_R],
            frame_torso, frame_arm
        );
        c->setLimit(
            static_cast<float>(M_PI) * 0.5f,
            static_cast<float>(M_PI) * 0.5f,
            static_cast<float>(M_PI) * 0.4f
        );
        world->addConstraint(c, true);
    }

    // ---- Quadril esquerdo: btConeTwistConstraint ─────────────
    {
        btTransform frame_torso; frame_torso.setIdentity();
        frame_torso.setOrigin(btVector3(-0.14f, -0.42f, 0.f));

        btTransform frame_leg; frame_leg.setIdentity();
        frame_leg.setOrigin(btVector3(0.f, 0.35f, 0.f));

        auto* c = new btConeTwistConstraint(
            *bodies[TORSO], *bodies[LEG_L],
            frame_torso, frame_leg
        );
        c->setLimit(
            static_cast<float>(M_PI) * 0.3f,
            static_cast<float>(M_PI) * 0.3f,
            static_cast<float>(M_PI) * 0.1f
        );
        world->addConstraint(c, true);
    }

    // ---- Quadril direito ────────────────────────────────────
    {
        btTransform frame_torso; frame_torso.setIdentity();
        frame_torso.setOrigin(btVector3(0.14f, -0.42f, 0.f));

        btTransform frame_leg; frame_leg.setIdentity();
        frame_leg.setOrigin(btVector3(0.f, 0.35f, 0.f));

        auto* c = new btConeTwistConstraint(
            *bodies[TORSO], *bodies[LEG_R],
            frame_torso, frame_leg
        );
        c->setLimit(
            static_cast<float>(M_PI) * 0.3f,
            static_cast<float>(M_PI) * 0.3f,
            static_cast<float>(M_PI) * 0.1f
        );
        world->addConstraint(c, true);
    }
}

void Ragdoll::apply_impulse(const btVector3& impulse, const btVector3& origin) {
    for (int i = 0; i < COUNT; ++i) {
        if (!bodies[i]) continue;
        // Desperta o body (pode estar dormindo)
        bodies[i]->activate(true);

        btVector3 rel = bodies[i]->getCenterOfMassPosition() - origin;
        float dist = rel.length();
        if (dist < 0.001f) dist = 0.001f;

        // Atenua o impulso com a distância (falloff quadrático)
        float scale = 1.f / (1.f + dist * dist * 0.5f);
        bodies[i]->applyCentralImpulse(impulse * scale);
    }
}

} // namespace sim
