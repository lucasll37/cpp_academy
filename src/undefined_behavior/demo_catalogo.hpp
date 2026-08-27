// =============================================================================
//  src/undefined_behavior/demo_catalogo.hpp
//
//  O que é comportamento indefinido
//  ────────────────────────────────
//  A definição do padrão é curta e implacável:
//
//      "behavior for which this document imposes no requirements"
//
//  Nenhuma exigência. O compilador pode gerar o que quiser, o programa pode
//  fazer o que quiser, e — o ponto que quase todo mundo perde — o efeito não
//  precisa se limitar ao trecho errado. Ele pode aparecer ANTES da linha do
//  erro, porque o compilador reorganizou o código assumindo que o UB não
//  aconteceria.
//
//  Tópicos:
//  • As quatro categorias que o padrão distingue
//  • Por que UB existe (não é descuido)
//  • O catálogo do que é UB
//  • ⚠ "funcionou na minha máquina" não significa nada
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <climits>
#include <string>

namespace demo_catalogo {

void demo_categorias() {
    fmt::print("\n── 1.1 As quatro categorias ──\n");

    fmt::print("      | categoria             | o compilador...        | exemplo        |\n");
    fmt::print("      |-----------------------|------------------------|----------------|\n");
    fmt::print("      | bem definido          | faz o que o padrão diz | 2 + 2          |\n");
    fmt::print("      | implementation-defined| escolhe E DOCUMENTA    | sizeof(int)    |\n");
    fmt::print("      | unspecified           | escolhe, sem documentar| ordem de aval. |\n");
    fmt::print("      | UNDEFINED             | não tem obrigação      | int overflow   |\n\n");

    fmt::print("      As três primeiras são seguras: o programa faz ALGO previsível\n");
    fmt::print("      ou pelo menos limitado. A quarta não tem limite.\n\n");

    fmt::print("      implementation-defined, neste toolchain:\n");
    fmt::print("        sizeof(int)          = {}\n", sizeof(int));
    fmt::print("        sizeof(long)         = {} (é 4 no Windows!)\n", sizeof(long));
    fmt::print("        char é signed?         {}\n", CHAR_MIN < 0);
    fmt::print("        (em ARM, `char` é UNSIGNED por padrão — código que\n");
    fmt::print("         assume o contrário quebra ao portar)\n\n");

    fmt::print("      unspecified — a ordem de avaliação de argumentos:\n");
    {
        int  n = 0;
        auto f = [&n] { return ++n; };
        // ⚠ Chamar f() três vezes na mesma expressão daria uma ordem NÃO
        //   especificada. O GCC avalia da direita para a esquerda; o MSVC,
        //   da esquerda para a direita. Isto foi medido em
        //   src/lambdas/demo_padroes.hpp e a surpresa foi imprimir 3 2 1.
        const int a = f(), b = f(), c = f();     // com variáveis, a ordem é fixa
        fmt::print("        com variáveis nomeadas: {} {} {} (ordem garantida)\n", a, b, c);
        fmt::print("        dentro de UMA chamada: print(\"{{}} {{}} {{}}\", f(), f(), f())\n");
        fmt::print("        → o GCC imprime 3 2 1. É legal, e é armadilha.\n");
    }
}

void demo_por_que_existe() {
    fmt::print("\n── 1.2 Por que UB existe ──\n");

    fmt::print("      Não é descuido do comitê. É uma TROCA deliberada:\n\n");

    fmt::print("      (a) DESEMPENHO. Se `i + 1 > i` pudesse ser falso (overflow),\n");
    fmt::print("          o compilador não poderia simplificar\n");
    fmt::print("            for (int i = 0; i <= n; ++i)\n");
    fmt::print("          nem promover `i` a `long` num registrador de 64 bits.\n");
    fmt::print("          Como overflow com sinal é UB, ele PODE assumir que não\n");
    fmt::print("          acontece — e o laço fica mais rápido.\n\n");

    fmt::print("      (b) PORTABILIDADE. O padrão precisa valer para máquinas com\n");
    fmt::print("          complemento de 1, palavras de 9 bits, e sem MMU. Definir\n");
    fmt::print("          o comportamento de tudo obrigaria o compilador a emular\n");
    fmt::print("          uma semântica que o hardware não tem.\n\n");

    fmt::print("      (c) CUSTO ZERO PARA QUEM NÃO USA. Definir \"acesso fora do\n");
    fmt::print("          vetor lança exceção\" exigiria checagem em TODO acesso —\n");
    fmt::print("          e o C++ não cobra por segurança que você não pediu.\n\n");

    fmt::print("      O C++20 já converteu um caso famoso: inteiros COM SINAL\n");
    fmt::print("      passaram a ser obrigatoriamente complemento de dois (P0907).\n");
    fmt::print("      O OVERFLOW deles, no entanto, CONTINUA sendo UB — a\n");
    fmt::print("      representação foi fixada, o overflow não.\n");
}

void demo_catalogo_ub() {
    fmt::print("\n── 1.3 O catálogo ──\n");

    fmt::print("      MEMÓRIA (o AddressSanitizer pega quase tudo daqui):\n");
    fmt::print("        • acesso fora dos limites de array/vector/string\n");
    fmt::print("        • use-after-free e use-after-scope\n");
    fmt::print("        • double free; delete[] em ponteiro de new (e vice-versa)\n");
    fmt::print("        • desreferência de nullptr\n");
    fmt::print("        • referência ligada a objeto que já morreu\n\n");

    fmt::print("      ARITMÉTICA (o UndefinedBehaviorSanitizer):\n");
    fmt::print("        • overflow de inteiro COM SINAL (sem sinal é definido!)\n");
    fmt::print("        • divisão por zero, e INT_MIN / -1\n");
    fmt::print("        • shift por >= a largura do tipo, ou por negativo\n");
    fmt::print("        • converter double para int fora da faixa\n\n");

    fmt::print("      TIPOS:\n");
    fmt::print("        • violar strict aliasing (seção 4)\n");
    fmt::print("        • ler um membro do union diferente do que foi escrito\n");
    fmt::print("          (isto é LEGAL em C, e UB em C++)\n");
    fmt::print("        • acesso desalinhado\n");
    fmt::print("        • usar um objeto antes do construtor ou depois do destrutor\n\n");

    fmt::print("      CONTROLE:\n");
    fmt::print("        • função com retorno não-void que termina sem `return`\n");
    fmt::print("        • data race (src/concurrency2/demo_memory_model.hpp)\n");
    fmt::print("        • laço infinito sem efeito colateral observável\n");
    fmt::print("        • recursão infinita\n");
    fmt::print("        • deixar exceção escapar de destrutor ou de `noexcept`\n\n");

    fmt::print("      BIBLIOTECA:\n");
    fmt::print("        • v[i] com i >= size()   (`.at(i)` lança; `v[i]` é UB)\n");
    fmt::print("        • desreferenciar `end()`, ou um iterador invalidado\n");
    fmt::print("        • `.front()` / `.back()` / `.pop_back()` em container vazio\n");
    fmt::print("        • passar nullptr para memcpy, mesmo com tamanho 0\n");
    fmt::print("        • `std::string_view` sobrevivendo à string que apontava\n");
}

void demo_funcionou() {
    fmt::print("\n── 1.4 ⚠ \"Funcionou na minha máquina\" ──\n");

    fmt::print("      UB não é uma falha que acontece. É uma PROMESSA QUEBRADA,\n");
    fmt::print("      e as consequências chegam quando o compilador decide\n");
    fmt::print("      aproveitar a promessa. Isso pode ser:\n\n");
    fmt::print("        • na próxima versão do compilador;\n");
    fmt::print("        • quando o -O2 entra no lugar do -O0;\n");
    fmt::print("        • quando uma função vira inline por causa de OUTRA mudança;\n");
    fmt::print("        • em outra arquitetura;\n");
    fmt::print("        • só em produção, sob carga.\n\n");

    fmt::print("      Um exemplo REAL medido neste subprojeto (seção 2): o mesmo\n");
    fmt::print("      use-after-free é DETECTADO pelo AddressSanitizer com -O0 e\n");
    fmt::print("      passa DESPERCEBIDO com -O1 — porque o otimizador eliminou a\n");
    fmt::print("      leitura antes de o sanitizer poder instrumentá-la.\n\n");

    fmt::print("      Corolário desagradável: um programa com UB pode passar em\n");
    fmt::print("      todos os testes, inclusive sob sanitizer, e ainda estar\n");
    fmt::print("      errado. A ausência de diagnóstico não é prova de correção.\n");
}

inline void run() {
    demo_categorias();
    demo_por_que_existe();
    demo_catalogo_ub();
    demo_funcionou();
}

}  // namespace demo_catalogo
