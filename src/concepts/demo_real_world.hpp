// =============================================================================
//  src/concepts/demo_real_world.hpp
//
//  Concepts em Casos Reais
//  ────────────────────────
//  Mostra como concepts melhoram código de produção:
//  • Algoritmos genéricos com requisitos explícitos
//  • Policy-based design com concepts
//  • Substituindo type erasure pesado por concepts leves
//  • Concepts para validação de configuração em compile-time
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <concepts>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <functional>
#include <sstream>

namespace demo_real_world {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Algoritmos genéricos com requisitos explícitos
//     O código fica autodocumentado: os concepts dizem exatamente
//     o que o algoritmo precisa do tipo.
// ─────────────────────────────────────────────────────────────────────────────

// Concept: tipo que suporta as operações necessárias para estatísticas
template <typename T>
concept Estatistico = std::floating_point<T> ||
    (std::integral<T> && !std::same_as<T, bool>);

// Concept: range iterável com value_type acessível
template <typename R>
concept RangeNumerico = requires(R r) {
    r.begin();
    r.end();
    requires Estatistico<typename R::value_type>;
};

template <RangeNumerico R>
auto media(const R& range) {
    using T = typename R::value_type;
    if (range.empty()) return static_cast<double>(0);
    auto soma = std::accumulate(range.begin(), range.end(), static_cast<double>(0));
    return soma / static_cast<double>(range.size());
}

template <RangeNumerico R>
auto variancia(const R& range) {
    if (range.size() < 2) return 0.0;
    double m = media(range);
    double soma_sq = 0.0;
    for (auto v : range) soma_sq += (v - m) * (v - m);
    return soma_sq / static_cast<double>(range.size() - 1);
}

template <RangeNumerico R>
auto mediana(R range) {   // cópia intencional para ordenar
    std::sort(range.begin(), range.end());
    std::size_t n = range.size();
    if (n == 0) return 0.0;
    if (n % 2 == 0)
        return (static_cast<double>(range[n/2 - 1]) + range[n/2]) / 2.0;
    return static_cast<double>(range[n/2]);
}

void demo_algoritmos() {
    fmt::print("\n── 4.1 Algoritmos genéricos com concepts ──\n");

    std::vector<int>    vi{3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
    std::vector<double> vd{2.5, 3.7, 1.1, 4.8, 2.2};

    fmt::print("  vi = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3]\n");
    fmt::print("    média:    {:.2f}\n", media(vi));
    fmt::print("    variância:{:.2f}\n", variancia(vi));
    fmt::print("    mediana:  {:.1f}\n", mediana(vi));

    fmt::print("  vd = [2.5, 3.7, 1.1, 4.8, 2.2]\n");
    fmt::print("    média:    {:.2f}\n", media(vd));
    fmt::print("    variância:{:.2f}\n", variancia(vd));
    fmt::print("    mediana:  {:.1f}\n", mediana(vd));
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Policy-based design com concepts
//     Policies definem comportamentos intercambiáveis.
//     Concepts garantem que qualquer policy implementa o contrato certo.
// ─────────────────────────────────────────────────────────────────────────────

// Concept: policy de serialização
template <typename P, typename T>
concept SerializacaoPolicy = requires(P policy, const T& obj) {
    { policy.serializar(obj) } -> std::same_as<std::string>;
    { policy.deserializar(std::string{}) } -> std::same_as<T>;
};

// Concept: policy de logging
template <typename L>
concept LoggingPolicy = requires(L logger, std::string_view msg) {
    logger.log(msg);
    { logger.prefixo() } -> std::convertible_to<std::string>;
};

struct LogConsole {
    void log(std::string_view msg) const {
        fmt::print("    [LOG] {}\n", msg);
    }
    std::string prefixo() const { return "CONSOLE"; }
};

struct LogSilencioso {
    void log(std::string_view) const {}   // descarta tudo
    std::string prefixo() const { return "SILENT"; }
};

// Componente que aceita qualquer policy de logging válida
template <LoggingPolicy Logger = LogConsole>
class ProcessadorDados {
public:
    explicit ProcessadorDados(Logger logger = {}) : logger_(std::move(logger)) {}

    std::vector<int> processar(std::vector<int> dados) {
        logger_.log(fmt::format("processando {} elementos", dados.size()));
        std::sort(dados.begin(), dados.end());
        logger_.log("ordenamento concluído");
        return dados;
    }

private:
    Logger logger_;
};

void demo_policy_based() {
    fmt::print("\n── 4.2 Policy-based design com concepts ──\n");

    fmt::print("  ProcessadorDados<LogConsole>:\n");
    ProcessadorDados<LogConsole> p1;
    auto r1 = p1.processar({5, 2, 8, 1, 9});
    fmt::print("  resultado: ");
    for (auto v : r1) fmt::print("{} ", v);
    fmt::print("\n");

    fmt::print("\n  ProcessadorDados<LogSilencioso>:\n");
    ProcessadorDados<LogSilencioso> p2;
    auto r2 = p2.processar({5, 2, 8, 1, 9});
    fmt::print("  resultado (sem log): ");
    for (auto v : r2) fmt::print("{} ", v);
    fmt::print("\n");

    fmt::print("\n  LoggingPolicy<LogConsole>:    {}\n", LoggingPolicy<LogConsole>);
    fmt::print("  LoggingPolicy<LogSilencioso>: {}\n", LoggingPolicy<LogSilencioso>);
    fmt::print("  LoggingPolicy<int>:           {}\n", LoggingPolicy<int>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Concept como documentação de contrato
//     Em vez de comentários, concepts tornam contratos verificáveis.
// ─────────────────────────────────────────────────────────────────────────────

// Concept: qualquer tipo que pode ser usado como chave em hash map
template <typename K>
concept ChaveHashavel = std::equality_comparable<K> && requires(K k) {
    { std::hash<K>{}(k) } -> std::convertible_to<std::size_t>;
};

// Concept: tipo que representa uma entidade de domínio com ID único
template <typename E>
concept EntidadeDominio = requires(E e) {
    { e.id() }   -> std::convertible_to<std::string>;
    { e.nome() } -> std::convertible_to<std::string>;
    requires std::copyable<E>;
    requires std::equality_comparable<E>;
};

struct Produto {
    std::string id_, nome_;
    double preco_;

    std::string id()   const { return id_; }
    std::string nome() const { return nome_; }
    bool operator==(const Produto& o) const { return id_ == o.id_; }
};

template <EntidadeDominio E>
void registrar(const E& entidade) {
    fmt::print("  registrado: id='{}', nome='{}'\n",
               entidade.id(), entidade.nome());
}

void demo_contratos() {
    fmt::print("\n── 4.3 Concepts como contratos de domínio ──\n");

    fmt::print("  ChaveHashavel<std::string>: {}\n", ChaveHashavel<std::string>);
    fmt::print("  ChaveHashavel<int>:         {}\n", ChaveHashavel<int>);
    fmt::print("  ChaveHashavel<Produto>:     {}\n", ChaveHashavel<Produto>); // sem hash

    fmt::print("\n  EntidadeDominio<Produto>:  {}\n", EntidadeDominio<Produto>);
    fmt::print("  EntidadeDominio<int>:       {}\n", EntidadeDominio<int>);

    registrar(Produto{"P001", "Teclado Mecânico", 450.0});
    registrar(Produto{"P002", "Monitor 4K", 2800.0});
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Concepts vs herança — quando usar cada um
// ─────────────────────────────────────────────────────────────────────────────

void demo_concepts_vs_heranca() {
    fmt::print("\n── 4.4 Concepts vs Herança — quando usar ──\n");
    fmt::print(
        "  ┌──────────────────────────┬──────────────────┬────────────────────┐\n"
        "  │                          │ Herança virtual  │ Concepts           │\n"
        "  ├──────────────────────────┼──────────────────┼────────────────────┤\n"
        "  │ Polimorfismo             │ runtime          │ compile-time       │\n"
        "  │ Coleção heterogênea      │ ✓ (Base*)        │ ✗                  │\n"
        "  │ Tipos de terceiros       │ ✗ (precisam herdar)│ ✓ (duck typing)  │\n"
        "  │ Overhead                 │ vtable pointer   │ zero               │\n"
        "  │ Erros                    │ runtime          │ compile-time       │\n"
        "  │ Expressividade           │ boa              │ excelente          │\n"
        "  ├──────────────────────────┼──────────────────┼────────────────────┤\n"
        "  │ Use quando...            │ runtime dispatch │ templates genéricos│\n"
        "  │                          │ coleções mistas  │ algoritmos         │\n"
        "  │                          │ plugins/dll      │ policies, mixins   │\n"
        "  └──────────────────────────┴──────────────────┴────────────────────┘\n"
    );
    fmt::print("  Conceito-chave: concepts fazem duck typing VERIFICADO em compile-time.\n");
    fmt::print("  Se int e std::string ambos satisfazem um concept, ambos funcionam\n");
    fmt::print("  sem precisar herdar de nada.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_algoritmos();
    demo_policy_based();
    demo_contratos();
    demo_concepts_vs_heranca();
}

} // namespace demo_real_world
