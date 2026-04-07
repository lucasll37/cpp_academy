// =============================================================================
//  src/ecs/world.hpp
//
//  World — o registro central do ECS
//  ───────────────────────────────────
//  World é a fachada que une EntityManager + ComponentStores.
//  É a única coisa com que o código externo precisa interagir.
//
//  Funcionalidades:
//  • criar_entidade / destruir_entidade
//  • adicionar<T> / remover<T> / get<T> / tem<T>
//  • query<T...> — itera entidades que têm todos os tipos pedidos
//
//  Type erasure via std::type_index:
//  Cada tipo de componente tem um ComponentStore<T> guardado em um map.
//  O tipo é apagado via IComponentStore* para o container, mas
//  recuperado com static_cast quando necessário.
// =============================================================================
#pragma once
#include "entity.hpp"
#include "component_store.hpp"
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <functional>
#include <tuple>

namespace ecs {

class World {
public:
    explicit World(uint32_t max_entities = 10000)
        : entities_(max_entities) {}

    // ── Entidades ─────────────────────────────────────────────────────────────
    Entity criar()          { return entities_.criar(); }
    void   destruir(Entity e) {
        // Remove todos os componentes da entidade antes de destruí-la
        for (auto& [tipo, store] : stores_)
            store->remover(e);
        entities_.destruir(e);
    }
    bool   existe(Entity e) const { return entities_.existe(e); }
    std::size_t num_entidades() const { return entities_.contagem(); }

    // ── Componentes ───────────────────────────────────────────────────────────

    // adicionar<T>(entity, args...) — constrói o componente in-place
    template <typename T, typename... Args>
    T& adicionar(Entity e, Args&&... args) {
        return store_de<T>().adicionar(e, std::forward<Args>(args)...);
    }

    // remover<T>(entity)
    template <typename T>
    void remover(Entity e) {
        store_de<T>().remover(e);
    }

    // get<T>(entity) — retorna referência ao componente
    template <typename T>
    T& get(Entity e) {
        return store_de<T>().get(e);
    }

    template <typename T>
    const T& get(Entity e) const {
        return store_const<T>().get(e);
    }

    // tem<T>(entity) — verifica se entidade tem o componente
    template <typename T>
    bool tem(Entity e) const {
        auto it = stores_.find(std::type_index(typeid(T)));
        if (it == stores_.end()) return false;
        return it->second->tem(e);
    }

    // ── Query ─────────────────────────────────────────────────────────────────
    // query<T1, T2, ...>(callback) — chama callback para cada entidade
    // que possui TODOS os componentes listados
    //
    // Exemplo:
    //   world.query<Posicao, Velocidade>([](Entity e, Posicao& p, Velocidade& v) {
    //       p.x += v.dx;
    //   });

    template <typename... Ts, typename Func>
    void query(Func&& func) {
        // Itera sobre todas as entidades do primeiro tipo (menor iteração possível)
        // Depois verifica se têm os demais tipos
        auto& primeiro_store = store_de<first_t<Ts...>>();

        const auto& ents = primeiro_store.entidades();
        for (std::size_t i = 0; i < ents.size(); ++i) {
            Entity e = ents[i];
            if (tem_todos<Ts...>(e)) {
                func(e, get<Ts>(e)...);
            }
        }
    }

    // Versão sem Entity no callback
    template <typename... Ts, typename Func>
    void query_components(Func&& func) {
        auto& primeiro_store = store_de<first_t<Ts...>>();
        const auto& ents = primeiro_store.entidades();
        for (std::size_t i = 0; i < ents.size(); ++i) {
            Entity e = ents[i];
            if (tem_todos<Ts...>(e)) {
                func(get<Ts>(e)...);
            }
        }
    }

    // Quantas entidades têm o componente T
    template <typename T>
    std::size_t contagem() {
        return store_de<T>().contagem();
    }

private:
    // ── Helpers de template ───────────────────────────────────────────────────

    // Extrai o primeiro tipo de um pack — alias não funciona com pack expansion,
    // precisa de struct helper com specialization
    template <typename... Ts> struct first_type;
    template <typename T, typename... Rest>
    struct first_type<T, Rest...> { using type = T; };

    template <typename... Ts>
    using first_t = typename first_type<Ts...>::type;

    // Verifica se entidade tem todos os tipos do pack
    template <typename... Ts>
    bool tem_todos(Entity e) const {
        return (tem<Ts>(e) && ...); // fold expression C++17
    }

    // Retorna (ou cria) o ComponentStore<T> para o tipo T
    template <typename T>
    ComponentStore<T>& store_de() {
        auto key = std::type_index(typeid(T));
        auto it = stores_.find(key);
        if (it == stores_.end()) {
            stores_[key] = std::make_unique<ComponentStore<T>>();
        }
        return static_cast<ComponentStore<T>&>(*stores_[key]);
    }

    template <typename T>
    const ComponentStore<T>& store_const() const {
        auto key = std::type_index(typeid(T));
        return static_cast<const ComponentStore<T>&>(*stores_.at(key));
    }

    EntityManager entities_;

    // type_index → ComponentStore (type-erased via IComponentStore)
    std::unordered_map<std::type_index,
                       std::unique_ptr<IComponentStore>> stores_;
};

} // namespace ecs
