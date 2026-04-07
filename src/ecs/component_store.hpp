// =============================================================================
//  src/ecs/component_store.hpp
//
//  ComponentStore<T> — armazena componentes de tipo T em array contíguo
//  ──────────────────────────────────────────────────────────────────────
//  Este é o coração do ECS e a razão de performance.
//
//  Abordagem naive (O QUE NÃO FAZEMOS):
//      std::unordered_map<Entity, T>
//      → cada componente alocado separadamente no heap
//      → acesso aleatório à memória → cache miss em todo loop
//
//  Nossa abordagem (O QUE FAZEMOS):
//      std::vector<T> denso (packed array)
//      + índice Entity → posição no array (sparse array)
//
//  Layout de memória:
//      componentes_: [T0][T1][T2][T3]...  ← CONTÍGUO, cache-friendly
//      entidades_:   [e2][e0][e5][e1]...  ← qual entity está em cada slot
//      indice_:      e0→1, e1→3, e2→0, e5→2  ← entity → slot
//
//  Resultado:
//      • Iterar todos os componentes = iterar array linear → CPU prefetch feliz
//      • get(entity) = O(1) via lookup no índice
//      • Sem fragmentação de heap
// =============================================================================
#pragma once
#include "entity.hpp"
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace ecs {

// Interface base — permite apagar componentes sem conhecer o tipo concreto
struct IComponentStore {
    virtual ~IComponentStore() = default;
    virtual void remover(Entity e) = 0;
    virtual bool tem(Entity e) const = 0;
};

// =============================================================================
//  ComponentStore<T> — packed array para componentes de tipo T
// =============================================================================
template <typename T>
class ComponentStore : public IComponentStore {
public:
    // Adiciona componente para a entidade — constrói in-place
    template <typename... Args>
    T& adicionar(Entity e, Args&&... args) {
        if (indice_.count(e))
            throw std::runtime_error("ComponentStore: entidade já tem este componente");

        indice_[e] = componentes_.size();
        entidades_.push_back(e);
        componentes_.emplace_back(std::forward<Args>(args)...);
        return componentes_.back();
    }

    // Remove componente — swap com último para manter array denso
    void remover(Entity e) override {
        if (!indice_.count(e)) return;

        std::size_t slot = indice_[e];
        std::size_t ultimo = componentes_.size() - 1;

        if (slot != ultimo) {
            // Move o último para o slot removido — mantém array denso
            componentes_[slot] = std::move(componentes_[ultimo]);
            Entity e_ultimo = entidades_[ultimo];
            entidades_[slot] = e_ultimo;
            indice_[e_ultimo] = slot;
        }

        componentes_.pop_back();
        entidades_.pop_back();
        indice_.erase(e);
    }

    // Acesso O(1) ao componente da entidade
    T& get(Entity e) {
        auto it = indice_.find(e);
        if (it == indice_.end())
            throw std::runtime_error("ComponentStore: entidade não tem este componente");
        return componentes_[it->second];
    }

    const T& get(Entity e) const {
        auto it = indice_.find(e);
        if (it == indice_.end())
            throw std::runtime_error("ComponentStore: entidade não tem este componente");
        return componentes_[it->second];
    }

    bool tem(Entity e) const override {
        return indice_.count(e) > 0;
    }

    // Iteração direta sobre todos os componentes — O(n) linear, cache-friendly
    std::vector<T>& todos() { return componentes_; }
    const std::vector<T>& todos() const { return componentes_; }

    // Qual entidade está em cada slot (paralelo ao array de componentes)
    const std::vector<Entity>& entidades() const { return entidades_; }

    std::size_t contagem() const { return componentes_.size(); }

private:
    std::vector<T>                       componentes_; // dados contíguos
    std::vector<Entity>                  entidades_;   // entity por slot
    std::unordered_map<Entity, std::size_t> indice_;   // entity → slot
};

} // namespace ecs
