// =============================================================================
//  src/value_semantics/demo_copy_elision.hpp
//
//  Copy Elision — RVO, NRVO e o move que nunca acontece
//  ────────────────────────────────────────────────────
//  A otimização mais lucrativa do C++ é a que ELIMINA a operação inteira.
//  Desde o C++17, parte dela deixou de ser "otimização" e virou REGRA da
//  linguagem: certos temporários simplesmente não existem mais.
//
//  Tópicos:
//  • RVO garantido (C++17) — retornar um prvalue
//  • NRVO — retornar uma variável nomeada (opcional, mas quase sempre feito)
//  • Os casos em que a elisão NÃO se aplica
//  • O anti-padrão `return std::move(x)` — que PIORA o código
//  • Medindo tudo com um tipo instrumentado
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <utility>

namespace demo_copy_elision {

// ─────────────────────────────────────────────────────────────────────────────
//  Um tipo que narra a própria vida
// ─────────────────────────────────────────────────────────────────────────────
struct Rastreado {
    std::string tag;

    explicit Rastreado(std::string t) : tag(std::move(t)) {
        fmt::print("      + ctor       '{}'\n", tag);
    }
    Rastreado(const Rastreado& o) : tag(o.tag) {
        fmt::print("      + CÓPIA      '{}'\n", tag);
        ++copias;
    }
    Rastreado(Rastreado&& o) noexcept : tag(std::move(o.tag)) {
        fmt::print("      + MOVE       '{}'\n", tag);
        ++moves;
    }
    ~Rastreado() { fmt::print("      - dtor       '{}'\n", tag); }

    static inline int copias = 0;
    static inline int moves  = 0;

    static void zerar() { copias = moves = 0; }
    static void placar(const char* rotulo) {
        fmt::print("    → {}: {} cópia(s), {} move(s)\n", rotulo, copias, moves);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. RVO garantido (C++17) — retornar um prvalue
//
//     `return Rastreado("x");` não constrói um temporário para depois copiar.
//     Desde o C++17 o prvalue é construído DIRETAMENTE no lugar do destino.
//     Isso vale até para tipos sem construtor de cópia nem de move.
// ─────────────────────────────────────────────────────────────────────────────
Rastreado rvo_garantido() {
    return Rastreado("rvo");     // prvalue → construído no destino final
}

void demo_rvo() {
    fmt::print("\n── 3.1 RVO garantido (C++17) ──\n");
    Rastreado::zerar();

    fmt::print("    Rastreado r = rvo_garantido();\n");
    Rastreado r = rvo_garantido();

    Rastreado::placar("RVO");
    fmt::print("    Zero. O objeto nasceu direto em `r`.\n");
}

// Prova de que é regra da linguagem e não otimização: um tipo IMÓVEL
// (sem cópia e sem move) ainda pode ser retornado por valor.
struct Imovel {
    int v;
    explicit Imovel(int x) : v(x) {}
    Imovel(const Imovel&)            = delete;
    Imovel(Imovel&&)                 = delete;
    Imovel& operator=(const Imovel&) = delete;
    Imovel& operator=(Imovel&&)      = delete;
};

Imovel faz_imovel() {
    return Imovel(42);          // compila: nenhuma cópia/move é sequer considerada
}

void demo_imovel() {
    fmt::print("\n── 3.2 Retornando um tipo sem cópia e sem move ──\n");

    Imovel i = faz_imovel();
    fmt::print("    Imovel tem cópia e move DELETADOS, e ainda assim\n");
    fmt::print("    `Imovel i = faz_imovel();` compila. v = {}\n", i.v);
    fmt::print("    Isso só é possível porque a elisão de prvalue é obrigatória.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. NRVO — retornar uma variável NOMEADA
//
//     Aqui a elisão é PERMITIDA, não obrigatória. GCC e Clang fazem em -O0
//     também, desde que haja um único caminho de retorno com o mesmo objeto.
// ─────────────────────────────────────────────────────────────────────────────
Rastreado nrvo() {
    Rastreado local("nrvo");     // variável nomeada
    return local;                // NRVO: `local` já foi construída no destino
}

void demo_nrvo() {
    fmt::print("\n── 3.3 NRVO — variável nomeada ──\n");
    Rastreado::zerar();

    fmt::print("    Rastreado r = nrvo();\n");
    Rastreado r = nrvo();

    Rastreado::placar("NRVO");
    fmt::print("    Se deu 0/0, o compilador aplicou NRVO.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Quando a elisão NÃO acontece
//
//     Múltiplos caminhos de retorno com objetos DIFERENTES impedem o NRVO:
//     o compilador não sabe, no momento de construir, qual dos dois vai voltar.
//     Aí entra a regra de fallback: um `return` de variável local é tratado
//     como RVALUE, então acontece um MOVE (não uma cópia).
// ─────────────────────────────────────────────────────────────────────────────
Rastreado sem_nrvo(bool escolha) {
    Rastreado a("ramo-A");
    Rastreado b("ramo-B");
    if (escolha) return a;       // dois objetos candidatos → NRVO inviável
    return b;                    // → move implícito
}

void demo_sem_elisao() {
    fmt::print("\n── 3.4 Quando a elisão não se aplica ──\n");
    Rastreado::zerar();

    fmt::print("    Rastreado r = sem_nrvo(true);\n");
    Rastreado r = sem_nrvo(true);

    Rastreado::placar("dois ramos");
    fmt::print("    Sem NRVO, mas ainda assim MOVE — não cópia.\n");
    fmt::print("    Retornar variável local trata a expressão como rvalue.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. O anti-padrão: `return std::move(x)`
//
//     A intenção é boa ("quero garantir o move"), o efeito é o oposto:
//     `std::move(local)` transforma a expressão num XVALUE, e a elisão
//     só se aplica ao NOME da variável. Você desliga o NRVO e força o move.
//     De ZERO operações você foi para UMA.
// ─────────────────────────────────────────────────────────────────────────────
// O GCC diagnostica isto com -Wpessimizing-move (que vem em -Wall). Silenciamos
// APENAS aqui, porque o warning é justamente o que queremos demonstrar.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpessimizing-move"
Rastreado com_move_desnecessario() {
    Rastreado local("anti-padrão");
    return std::move(local);     // ✗ pessimização: mata o NRVO
}
#pragma GCC diagnostic pop

void demo_anti_padrao() {
    fmt::print("\n── 3.5 ✗ `return std::move(x)` piora o código ──\n");
    Rastreado::zerar();

    fmt::print("    Rastreado r = com_move_desnecessario();\n");
    Rastreado r = com_move_desnecessario();

    Rastreado::placar("return std::move(local)");
    fmt::print("    Compare com 3.3 (NRVO): lá foi 0/0, aqui foi 0/1.\n");
    fmt::print("    Regra: NUNCA escreva std::move no return de uma local.\n");
    fmt::print("    (GCC avisa com -Wpessimizing-move / -Wredundant-move)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Passagem de parâmetro — onde a elisão também trabalha
// ─────────────────────────────────────────────────────────────────────────────
void consome_por_valor(Rastreado r) {
    fmt::print("      (consumido: '{}')\n", r.tag);
}

void demo_parametros() {
    fmt::print("\n── 3.6 Elisão na passagem de parâmetro ──\n");

    fmt::print("    consome_por_valor(Rastreado(\"temp\"));   // prvalue\n");
    Rastreado::zerar();
    consome_por_valor(Rastreado("temp"));
    Rastreado::placar("prvalue como argumento");

    fmt::print("\n    Rastreado nomeado(\"nomeado\");\n");
    Rastreado nomeado("nomeado");

    fmt::print("    consome_por_valor(nomeado);              // lvalue → cópia\n");
    Rastreado::zerar();
    consome_por_valor(nomeado);
    Rastreado::placar("lvalue como argumento");

    fmt::print("\n    consome_por_valor(std::move(nomeado));   // xvalue → move\n");
    Rastreado::zerar();
    consome_por_valor(std::move(nomeado));
    Rastreado::placar("xvalue como argumento");

    fmt::print("\n    Aqui `std::move` é legítimo: o objeto não é o valor de retorno\n");
    fmt::print("    e nós de fato não vamos mais usá-lo.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_rvo();
    demo_imovel();
    demo_nrvo();
    demo_sem_elisao();
    demo_anti_padrao();
    demo_parametros();
}

} // namespace demo_copy_elision
