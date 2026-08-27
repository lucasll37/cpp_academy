#include "rope.hpp"
#include <BulletSoftBody/btSoftBodyHelpers.h>

namespace sim {

void Rope::build(PhysicsWorld& pw,
                 float x0, float y0,
                 float x1, float y1,
                 int segments,
                 btRigidBody* attach)
{
    // btSoftBodyHelpers::CreateRope:
    //   world_info, from, to, res (número de segmentos), fixeds (bitmask)
    //
    //   fixeds = 1 → âncora o nó 0
    //   fixeds = 2 → âncora o último nó
    //   fixeds = 3 → âncora ambos
    //
    //   Âncora via fixeds trava o nó à posição inicial no espaço mundial.
    //   Para ancorar a um body dinâmico, use appendAnchor() depois.

    body = btSoftBodyHelpers::CreateRope(
        pw.soft_info(),
        btVector3(x0, y0, 0.f),
        btVector3(x1, y1, 0.f),
        segments,
        1  // âncora só o nó 0 (topo)
    );

    // ── Material da corda ─────────────────────────────────────
    //
    // btSoftBody::Material guarda os parâmetros de deformação:
    //   m_kLST (Linear Stiffness): resistência ao alongamento [0,1]
    //          1.0 = inextensível, 0.0 = borracha sem tensão
    //   m_kAST (Angular Stiffness): resistência à curvatura (corda: não usamos)
    //   m_kVST (Volume Stiffness):  para soft bodies 3D (não relevante aqui)
    //
    // kDP (damping) fica em body->m_cfg.kDP — amortecimento geral [0,1]
    // kDG (drag)    em body->m_cfg.kDG   — arrasto do fluido (ar/água)

    btSoftBody::Material* mat = body->m_materials[0];
    mat->m_kLST = 0.8f;    // corda semi-rígida
    mat->m_kAST = 0.5f;

    body->m_cfg.kDP     = 0.05f;  // damping leve
    body->m_cfg.kDG     = 0.01f;  // leve arrasto do ar
    body->m_cfg.kSHR    = 1.0f;   // shear stiffness
    body->m_cfg.kCHR    = 1.0f;   // rigid contact hardness

    // Massa distribuída por nó (total da corda dividido por nós)
    body->setTotalMass(1.0f, true);

    // gera links internos para resistência à curvatura
    body->generateBendingConstraints(2);

    // Se passamos um body para anexar, âncora o último nó a ele.
    // appendAnchor(nodeIndex, rigidBody, disableCollision, influence)
    //   influence: 0 = corda não influencia o body, 1 = influencia totalmente
    if (attach) {
        int last = body->m_nodes.size() - 1;
        body->appendAnchor(last, attach, false, 1.0f);
    }

    pw.add_soft(body);
}

} // namespace sim
