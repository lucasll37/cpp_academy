// =============================================================================
//  src/ecs/entity.hpp
//
//  Entity — apenas um ID
//  ──────────────────────
//  Uma Entity é simplesmente um número inteiro único.
//  Não tem dados, não tem comportamento — só uma identidade.
//
//  EntityManager:
//  • Gera IDs únicos e crescentes
//  • Reutiliza IDs de entidades destruídas (free list)
//  • Controla quais entidades estão vivas
// =============================================================================
#pragma once
#include <cstdint>
#include <vector>
#include <queue>
#include <stdexcept>
#include <algorithm>  // std::find

namespace ecs {

// Entity é só um alias para uint32_t — sem overhead, sem vtable, nada
using Entity = uint32_t;

constexpr Entity ENTITY_NULL = UINT32_MAX; // sentinela para "entidade inválida"

// =============================================================================
//  EntityManager — gera e recicla IDs
// =============================================================================
class EntityManager {
public:
    explicit EntityManager(uint32_t max_entities = 10000)
        : max_(max_entities), proximo_(0) {}

    // Cria uma nova entidade — reutiliza ID destruído se disponível
    Entity criar() {
        if (!livres_.empty()) {
            Entity id = livres_.front();
            livres_.pop();
            vivos_.push_back(id);
            return id;
        }
        if (proximo_ >= max_)
            throw std::runtime_error("EntityManager: limite de entidades atingido");
        Entity id = proximo_++;
        vivos_.push_back(id);
        return id;
    }

    // Destrói uma entidade — ID volta para a fila de reutilização
    void destruir(Entity e) {
        auto it = std::find(vivos_.begin(), vivos_.end(), e);
        if (it != vivos_.end()) {
            vivos_.erase(it);
            livres_.push(e);
        }
    }

    bool existe(Entity e) const {
        return std::find(vivos_.begin(), vivos_.end(), e) != vivos_.end();
    }

    const std::vector<Entity>& todos() const { return vivos_; }
    std::size_t contagem() const { return vivos_.size(); }

private:
    uint32_t             max_;
    uint32_t             proximo_;
    std::vector<Entity>  vivos_;
    std::queue<Entity>   livres_;   // IDs reciclados
};

} // namespace ecs
