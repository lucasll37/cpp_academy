#include <btBulletDynamicsCommon.h>
#include <fmt/core.h>

// ============================================================
// BULLET3 — simulação de queda livre com colisão
//
// Cenário: uma esfera cai de y=10 e colide com um plano em y=0.
// A cada step imprimimos a posição Y da esfera.
//
// Pipeline de um frame:
//   1. dynamicsWorld->stepSimulation(dt, maxSubSteps)
//        ↳ broadphase   — detecta pares de AABB sobrepostos
//        ↳ narrowphase  — colisão exata entre os pares
//        ↳ solver       — resolve contatos (impulsos, fricção)
//        ↳ integração   — atualiza posição/velocidade de cada corpo
//   2. ler posição via motionState->getWorldTransform()
// ============================================================

int main() {
    // ── 1. configuração do mundo ──────────────────────────────
    //
    // btDefaultCollisionConfiguration: registra os algoritmos
    // padrão para cada par de shapes (sphere-sphere, box-sphere, etc.)
    auto* config     = new btDefaultCollisionConfiguration();

    // btCollisionDispatcher: itera os pares e chama o algoritmo certo.
    // É aqui que acontece o narrowphase.
    auto* dispatcher = new btCollisionDispatcher(config);

    // btDbvtBroadphase: broadphase baseado em Dynamic AABB Tree (DBVT).
    // Muito eficiente para cenas dinâmicas — O(n log n) por frame.
    // Alternativa: btAxisSweep3 para cenas com limites fixos.
    auto* broadphase = new btDbvtBroadphase();

    // btSequentialImpulseConstraintSolver: o solver padrão.
    // Usa Projected Gauss-Seidel iterativo para resolver contatos e joints.
    auto* solver     = new btSequentialImpulseConstraintSolver();

    // o mundo que une tudo:
    auto* world      = new btDiscreteDynamicsWorld(
        dispatcher, broadphase, solver, config
    );

    // gravidade padrão da Terra em m/s²
    world->setGravity(btVector3(0, -9.81f, 0));


    // ── 2. plano estático (chão) ──────────────────────────────
    //
    // btStaticPlaneShape: plano infinito, muito eficiente.
    // Normal (0,1,0) = plano horizontal. Offset = 0.
    // Massa = 0 → estático, nunca se move.
    auto* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);

    btTransform groundTransform;
    groundTransform.setIdentity();
    groundTransform.setOrigin(btVector3(0, 0, 0));

    // btDefaultMotionState: guarda a transformação do corpo.
    // Para corpos estáticos, é só um container de posição.
    auto* groundMotion = new btDefaultMotionState(groundTransform);

    // massa = 0, inércia = (0,0,0) → corpo estático
    btRigidBody::btRigidBodyConstructionInfo groundCI(
        0, groundMotion, groundShape, btVector3(0, 0, 0)
    );
    auto* groundBody = new btRigidBody(groundCI);
    world->addRigidBody(groundBody);


    // ── 3. esfera dinâmica ────────────────────────────────────
    //
    // btSphereShape: shape mais simples para corpos convexos.
    // A colisão esfera-plano é O(1) — caso mais barato possível.
    auto* sphereShape = new btSphereShape(1.0f); // raio = 1 metro

    btTransform sphereTransform;
    sphereTransform.setIdentity();
    sphereTransform.setOrigin(btVector3(0, 10, 0)); // começa em y=10

    btScalar mass = 1.0f; // 1 kg

    // calculateLocalInertia: calcula o tensor de inércia da shape
    // para a massa dada. Para uma esfera: I = (2/5) * m * r²
    btVector3 localInertia(0, 0, 0);
    sphereShape->calculateLocalInertia(mass, localInertia);

    auto* sphereMotion = new btDefaultMotionState(sphereTransform);

    btRigidBody::btRigidBodyConstructionInfo sphereCI(
        mass, sphereMotion, sphereShape, localInertia
    );
    auto* sphereBody = new btRigidBody(sphereCI);

    // restitution: coeficiente de elasticidade (0 = sem quique, 1 = perfeito)
    sphereBody->setRestitution(0.6f);
    groundBody->setRestitution(0.6f);

    world->addRigidBody(sphereBody);


    // ── 4. loop de simulação ──────────────────────────────────
    //
    // stepSimulation(timeStep, maxSubSteps, fixedTimeStep):
    //   - timeStep:      tempo real decorrido desde o último frame
    //   - maxSubSteps:   quantos sub-passos internos são permitidos
    //                    se timeStep > fixedTimeStep
    //   - fixedTimeStep: o dt interno fixo (padrão: 1/60 s)
    //
    // Bullet usa um dt fixo internamente para estabilidade numérica.
    // Se você passa timeStep > fixedTimeStep, ele subdivide.
    // Se você passa maxSubSteps = 1 e timeStep fixo, fica determinístico.

    fmt::println("{:>6}  {:>10}  {:>12}  {:>12}",
        "step", "time (s)", "y (m)", "vy (m/s)");
    fmt::println("{}", std::string(46, '-'));

    const float dt       = 1.0f / 60.0f;
    const int   steps    = 180; // 3 segundos de simulação

    for (int i = 0; i < steps; ++i) {
        world->stepSimulation(dt, 1, dt);

        // getWorldTransform via motionState — inclui interpolação
        btTransform t;
        sphereBody->getMotionState()->getWorldTransform(t);

        float y  = t.getOrigin().getY();
        float vy = sphereBody->getLinearVelocity().getY();

        // imprime a cada 10 frames
        if (i % 10 == 0) {
            fmt::println("{:>6}  {:>10.3f}  {:>12.4f}  {:>12.4f}",
                i, i * dt, y, vy);
        }
    }


    // ── 5. cleanup ────────────────────────────────────────────
    //
    // A ordem importa: remover corpos antes de deletar shapes/motionStates.
    // Bullet não faz ownership — você é responsável por tudo.
    world->removeRigidBody(sphereBody);
    world->removeRigidBody(groundBody);

    delete sphereBody;   delete sphereMotion;   delete sphereShape;
    delete groundBody;   delete groundMotion;   delete groundShape;
    delete world;        delete solver;
    delete broadphase;   delete dispatcher;     delete config;

    return 0;
}