// =============================================================================
//  src/stl/demo_containers.hpp
//
//  Containers — escolher pela COMPLEXIDADE, não pelo hábito
//  ────────────────────────────────────────────────────────
//  A STL oferece uma dúzia de containers. Escolher errado não gera erro de
//  compilação nem resultado errado — gera lentidão silenciosa que só aparece
//  quando os dados crescem.
//
//  Regra que resolve a maioria dos casos: use `std::vector`. Só troque quando
//  puder NOMEAR a operação cuja complexidade te incomoda.
//
//  Tópicos:
//  • As famílias e suas complexidades
//  • vector: capacity vs size, reserve, o crescimento amortizado
//  • Invalidação de iteradores — a tabela que importa
//  • emplace vs push — o que realmente muda
//  • Associativos ordenados vs hash
//  • Por que vector vence list mesmo com inserção no meio
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace demo_containers {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O mapa das complexidades
// ─────────────────────────────────────────────────────────────────────────────
void demo_panorama() {
    fmt::print("\n── 1.1 Complexidade por container ──\n");

    fmt::print("      | container          | acesso | inserção  | busca    | memória |\n");
    fmt::print("      |--------------------|--------|-----------|----------|---------|\n");
    fmt::print("      | vector             | O(1)   | O(1) fim* | O(n)     | contígua|\n");
    fmt::print("      | deque              | O(1)   | O(1) 2pts | O(n)     | blocos  |\n");
    fmt::print("      | list               | O(n)   | O(1) c/it | O(n)     | nós     |\n");
    fmt::print("      | map / set          | —      | O(log n)  | O(log n) | nós     |\n");
    fmt::print("      | unordered_map/set  | —      | O(1) méd. | O(1) méd.| baldes  |\n");
    fmt::print("      * amortizado — ver 1.2\n");

    fmt::print("\n      Default: `vector`. A localidade de cache costuma vencer\n");
    fmt::print("      a complexidade assintótica em tamanhos reais (ver 1.6).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. vector — size, capacity e o crescimento amortizado
//
//     `size`     = quantos elementos existem
//     `capacity` = para quantos há espaço antes de realocar
//
//     Ao encher, o vector aloca um buffer MAIOR (o GCC dobra), move tudo e
//     libera o antigo. Como o buffer dobra, o custo total de N inserções é
//     O(N) — daí "O(1) amortizado".
// ─────────────────────────────────────────────────────────────────────────────
void demo_capacity() {
    fmt::print("\n── 1.2 vector: size vs capacity ──\n");

    std::vector<int> v;
    std::size_t cap_anterior = v.capacity();

    fmt::print("      inserindo 20 elementos, marcando as realocações:\n      ");
    for (int i = 0; i < 20; ++i) {
        v.push_back(i);
        if (v.capacity() != cap_anterior) {
            fmt::print("{}→{} ", cap_anterior, v.capacity());
            cap_anterior = v.capacity();
        }
    }
    fmt::print("\n      size={} capacity={}\n", v.size(), v.capacity());
    fmt::print("      ↑ o buffer DOBRA a cada estouro (no libstdc++)\n");

    // reserve elimina todas as realocações quando o tamanho é conhecido
    std::vector<int> r;
    r.reserve(20);
    std::size_t realocacoes = 0;
    std::size_t cap = r.capacity();
    for (int i = 0; i < 20; ++i) {
        r.push_back(i);
        if (r.capacity() != cap) { ++realocacoes; cap = r.capacity(); }
    }
    fmt::print("      com reserve(20): {} realocações\n", realocacoes);

    // `shrink_to_fit` devolve a memória excedente (é uma REQUISIÇÃO, não ordem)
    fmt::print("      antes do shrink_to_fit: capacity={}\n", v.capacity());
    v.shrink_to_fit();
    fmt::print("      após  o shrink_to_fit: capacity={}\n", v.capacity());

    // `clear` zera o size mas NÃO devolve a memória
    v.clear();
    fmt::print("      após clear(): size={} capacity={}  ← memória mantida\n",
               v.size(), v.capacity());
    fmt::print("      Para realmente liberar: `std::vector<int>{{}}.swap(v);`\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Invalidação de iteradores
//
//     Um iterador, referência ou ponteiro para um elemento pode deixar de ser
//     válido quando o container muda. Usar depois disso é UNDEFINED BEHAVIOR —
//     e é o bug de STL mais comum que existe.
// ─────────────────────────────────────────────────────────────────────────────
void demo_invalidacao() {
    fmt::print("\n── 1.3 Invalidação de iteradores ──\n");

    std::vector<int> v{1, 2, 3};
    v.reserve(10);                        // garante que NÃO vai realocar

    int* p = &v[0];
    fmt::print("      &v[0] antes  = {}\n", static_cast<void*>(p));
    v.push_back(4);                       // cabe na capacity → sem realocação
    fmt::print("      &v[0] depois = {} (sem realocação: ponteiro ainda válido)\n",
               static_cast<void*>(&v[0]));

    // Agora forçamos a realocação
    std::vector<int> w{1, 2, 3};          // capacity == 3
    int* q = &w[0];
    fmt::print("      &w[0] antes  = {}\n", static_cast<void*>(q));
    w.push_back(4);                       // estourou → REALOCOU
    fmt::print("      &w[0] depois = {} ← endereço mudou!\n", static_cast<void*>(&w[0]));
    fmt::print("      `q` agora aponta para memória liberada. Usá-lo é UB.\n");
    fmt::print("      (o AddressSanitizer detecta como heap-use-after-free)\n");

    fmt::print("\n      | operação                | invalida                        |\n");
    fmt::print("      |-------------------------|---------------------------------|\n");
    fmt::print("      | vector: realocação       | TUDO                            |\n");
    fmt::print("      | vector: erase(it)        | de `it` até o fim               |\n");
    fmt::print("      | deque: insert/erase meio | TUDO                            |\n");
    fmt::print("      | deque: push_front/back   | iteradores sim, referências não |\n");
    fmt::print("      | list: qualquer           | só o elemento removido          |\n");
    fmt::print("      | map/set: insert          | nada                            |\n");
    fmt::print("      | map/set: erase           | só o elemento removido          |\n");
    fmt::print("      | unordered_*: rehash      | iteradores sim, referências não  |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. O erro clássico: apagar dentro do loop
// ─────────────────────────────────────────────────────────────────────────────
void demo_erase_no_loop() {
    fmt::print("\n── 1.4 Apagar durante a iteração ──\n");

    // ✗ ERRADO: `it` é invalidado por erase, e `++it` opera em lixo
    // for (auto it = v.begin(); it != v.end(); ++it)
    //     if (*it % 2 == 0) v.erase(it);

    // ✓ CERTO: erase DEVOLVE o iterador para o próximo elemento
    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};
    for (auto it = v.begin(); it != v.end(); ) {
        if (*it % 2 == 0) it = v.erase(it);      // sem ++ aqui
        else              ++it;
    }
    fmt::print("      após remover pares (erase manual): {}\n", fmt::join(v, " "));

    // ✓ MELHOR: erase-remove (e, desde C++20, std::erase_if)
    std::vector<int> w{1, 2, 3, 4, 5, 6, 7, 8};
    std::erase_if(w, [](int x) { return x % 2 == 0; });   // C++20
    fmt::print("      após std::erase_if (C++20):        {}\n", fmt::join(w, " "));

    fmt::print("      ⚠  `std::remove` sozinho NÃO remove nada — ver demo_pitfalls.hpp\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. emplace vs push — o que realmente muda
//
//     push_back(x)     → constrói o objeto, depois copia/move para dentro
//     emplace_back(a,b)→ constrói o objeto DIRETO no lugar final
//
//     A diferença só aparece quando o objeto é caro e você tem os ARGUMENTOS,
//     não o objeto pronto. Com um objeto já construído, `push_back(std::move(x))`
//     e `emplace_back(std::move(x))` fazem exatamente o mesmo.
// ─────────────────────────────────────────────────────────────────────────────
struct Caro {
    std::string a, b;
    Caro(std::string x, std::string y) : a(std::move(x)), b(std::move(y)) { ++construcoes; }
    Caro(const Caro& o) : a(o.a), b(o.b) { ++copias; }
    Caro(Caro&& o) noexcept : a(std::move(o.a)), b(std::move(o.b)) { ++moves; }

    static inline int construcoes = 0, copias = 0, moves = 0;
    static void zerar() { construcoes = copias = moves = 0; }
    static void placar(const char* r) {
        fmt::print("      {:<34} ctor={} cópias={} moves={}\n", r, construcoes, copias, moves);
    }
};

void demo_emplace() {
    fmt::print("\n── 1.5 emplace vs push ──\n");

    {
        std::vector<Caro> v; v.reserve(4);
        Caro::zerar();
        v.push_back(Caro("a", "b"));           // constrói temporário + MOVE
        Caro::placar("push_back(Caro(...))");
    }
    {
        std::vector<Caro> v; v.reserve(4);
        Caro::zerar();
        v.emplace_back("a", "b");              // constrói DIRETO no lugar
        Caro::placar("emplace_back(\"a\",\"b\")");
    }
    {
        std::vector<Caro> v; v.reserve(4);
        Caro c("a", "b");
        Caro::zerar();
        v.push_back(c);                        // lvalue → CÓPIA
        Caro::placar("push_back(lvalue)");
    }
    {
        std::vector<Caro> v; v.reserve(4);
        Caro c("a", "b");
        Caro::zerar();
        v.push_back(std::move(c));             // rvalue → MOVE
        Caro::placar("push_back(std::move(c))");
    }

    fmt::print("      → emplace ganha quando você tem os ARGUMENTOS.\n");
    fmt::print("      → com um objeto pronto, push_back(std::move(x)) é idêntico.\n");
    fmt::print("      ⚠  emplace_back não é sempre melhor: ele aceita conversões\n");
    fmt::print("         EXPLÍCITAS silenciosamente, porque chama o ctor direto.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Associativos: ordenado (map) vs hash (unordered_map)
// ─────────────────────────────────────────────────────────────────────────────
void demo_associativos() {
    fmt::print("\n── 1.6 map vs unordered_map ──\n");

    std::map<std::string, int>           ordenado{{"pêra", 3}, {"maçã", 1}, {"uva", 2}};
    std::unordered_map<std::string, int> hash    {{"pêra", 3}, {"maçã", 1}, {"uva", 2}};

    fmt::print("      map (ordem = comparação da chave)      : ");
    for (const auto& [k, val] : ordenado) fmt::print("{}={} ", k, val);
    fmt::print("\n      unordered_map (ordem = balde do hash)  : ");
    for (const auto& [k, val] : hash) fmt::print("{}={} ", k, val);
    fmt::print("\n");

    // O acesso e a inserção têm quatro formas, com semânticas diferentes
    fmt::print("\n      as quatro formas de inserir/acessar:\n");

    ordenado["kiwi"] = 4;                       // INSERE se não existir
    fmt::print("        m[k] = v         → insere ou sobrescreve (e exige default ctor)\n");

    auto [it1, novo1] = ordenado.insert({"maçã", 99});
    fmt::print("        insert           → não sobrescreve; novo={} (maçã continua {})\n",
               novo1, it1->second);

    auto [it2, novo2] = ordenado.insert_or_assign("maçã", 99);
    fmt::print("        insert_or_assign → sobrescreve;      novo={} (maçã agora {})\n",
               novo2, it2->second);

    auto [it3, novo3] = ordenado.try_emplace("maçã", 1);
    fmt::print("        try_emplace      → só constrói se faltar; novo={}\n", novo3);

    fmt::print("\n      ⚠  `m[k]` num map const NÃO COMPILA — porque ele insere.\n");
    fmt::print("         Para só consultar, use `.at(k)` (lança) ou `.find(k)`.\n");

    fmt::print("\n      | prefira            | quando                              |\n");
    fmt::print("      |--------------------|-------------------------------------|\n");
    fmt::print("      | map / set          | precisa de ORDEM, ou range por chave|\n");
    fmt::print("      | unordered_map/set  | só precisa de busca — é mais rápido |\n");
    fmt::print("      | vector de pares    | poucos itens (< ~30) — cache vence   |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. A localidade de cache vence a complexidade
//
//     `list` insere no meio em O(1) e `vector` em O(n). Mesmo assim, o vector
//     ganha em quase todo tamanho real — porque o O(1) da list exige ACHAR a
//     posição (O(n) de saltos por ponteiro, cada um um cache miss), enquanto o
//     O(n) do vector é um memmove contíguo, que o hardware faz voando.
// ─────────────────────────────────────────────────────────────────────────────
void demo_cache_vs_complexidade() {
    fmt::print("\n── 1.7 Localidade de cache vs complexidade ──\n");

    constexpr int N = 20'000;
    using clock = std::chrono::steady_clock;

    // Inserção ordenada num vector (memmove a cada inserção)
    auto t0 = clock::now();
    std::vector<int> v;
    for (int i = 0; i < N; ++i) {
        auto pos = std::lower_bound(v.begin(), v.end(), i % 1000);
        v.insert(pos, i % 1000);
    }
    auto t1 = clock::now();

    // Inserção ordenada numa list (O(1) para inserir, O(n) para achar)
    std::list<int> l;
    for (int i = 0; i < N; ++i) {
        auto pos = l.begin();
        while (pos != l.end() && *pos < i % 1000) ++pos;
        l.insert(pos, i % 1000);
    }
    auto t2 = clock::now();

    using ms = std::chrono::milliseconds;
    fmt::print("      {} inserções ordenadas:\n", N);
    fmt::print("        vector (memmove contíguo) : {:>5} ms\n",
               std::chrono::duration_cast<ms>(t1 - t0).count());
    fmt::print("        list   (salto de ponteiro): {:>5} ms\n",
               std::chrono::duration_cast<ms>(t2 - t1).count());

    fmt::print("      A `list` tem a MELHOR complexidade e o PIOR desempenho.\n");
    fmt::print("      Use `list` só quando precisar de: referências estáveis,\n");
    fmt::print("      splice O(1), ou inserção/remoção com iterador já em mãos.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_panorama();
    demo_capacity();
    demo_invalidacao();
    demo_erase_no_loop();
    demo_emplace();
    demo_associativos();
    demo_cache_vs_complexidade();
}

} // namespace demo_containers
