// =============================================================================
//  src/undefined_behavior/demo_sanitizers.hpp
//
//  Os sanitizers — o que cada um realmente pega
//  ────────────────────────────────────────────
//  Sanitizers são instrumentação em tempo de compilação: o compilador insere
//  checagens em torno de cada acesso, e uma biblioteca de runtime mantém o
//  mapa do que é válido. Custam desempenho e memória, e por isso não vão para
//  produção — vão para os TESTES.
//
//  ⚠ A tabela desta seção NÃO é copiada de documentação. Cada linha foi
//    obtida rodando ub_exemplos.cpp deste subprojeto com o sanitizer
//    correspondente, neste toolchain (GCC 13.3 e clang 18.1.3). Três
//    resultados contrariaram o que se costuma afirmar.
//
//  Tópicos:
//  • Os quatro sanitizers
//  • A matriz de detecção MEDIDA
//  • ⚠ O que NENHUM sanitizer pega
//  • Como rodar
// =============================================================================
#pragma once
#include <fmt/core.h>

namespace demo_sanitizers {

void demo_quais() {
    fmt::print("\n── 3.1 Os quatro sanitizers ──\n");

    fmt::print("      | sanitizer | flag                   | pega              | custo |\n");
    fmt::print("      |-----------|------------------------|-------------------|-------|\n");
    fmt::print("      | Address   | -fsanitize=address     | memória           | ~2×   |\n");
    fmt::print("      | Undefined | -fsanitize=undefined   | UB aritmético     | ~1.2× |\n");
    fmt::print("      | Thread    | -fsanitize=thread      | data races        | 5-15× |\n");
    fmt::print("      | Memory    | -fsanitize=memory      | não inicializado  | ~3×   |\n\n");

    fmt::print("      O ASan também traz o LeakSanitizer, ligado por padrão no\n");
    fmt::print("      Linux — ele reporta no fim do programa o que não foi\n");
    fmt::print("      liberado. Desligue com ASAN_OPTIONS=detect_leaks=0.\n\n");

    fmt::print("      ⚠  ASan e TSan são INCOMPATÍVEIS entre si: os dois\n");
    fmt::print("         instrumentam os mesmos acessos. Dois jobs no CI.\n");
    fmt::print("      ⚠  MSan exige que TODA a pilha esteja instrumentada,\n");
    fmt::print("         inclusive a libstdc++. Sem uma libc++ compilada com\n");
    fmt::print("         MSan, ele produz falsos positivos ou — como medido\n");
    fmt::print("         aqui — simplesmente não reporta. É o menos usável dos\n");
    fmt::print("         quatro, e o único que o GCC não implementa.\n\n");

    fmt::print("      ASan+UBSan JUNTOS é a combinação padrão: barata, compatível\n");
    fmt::print("      e cobre a grande maioria. É a que este repositório usa para\n");
    fmt::print("      validar todo subprojeto.\n");
}

void demo_matriz() {
    fmt::print("\n── 3.2 A matriz de detecção, MEDIDA ──\n");

    fmt::print("      Rodando ub_exemplos.cpp com -fsanitize=address,undefined:\n\n");
    fmt::print("      | caso          | -O0             | -O1             |\n");
    fmt::print("      |---------------|-----------------|-----------------|\n");
    fmt::print("      | heap          | ASan: overflow  | ASan: overflow  |\n");
    fmt::print("      | pilha         | UBSan: index    | UBSan: index    |\n");
    fmt::print("      | uaf           | ASan: UAF       | ⚠ NADA          |\n");
    fmt::print("      | dangling      | UBSan: null ref | UBSan: null ref |\n");
    fmt::print("      | overflow      | UBSan: overflow | UBSan: overflow |\n");
    fmt::print("      | shift         | UBSan: shift    | UBSan: shift    |\n");
    fmt::print("      | divisao       | UBSan: div zero | UBSan: div zero |\n");
    fmt::print("      | nulo          | UBSan: null     | UBSan: null     |\n");
    fmt::print("      | vazamento     | LSan: leak      | LSan: leak      |\n");
    fmt::print("      | aliasing      | ⚠ NADA          | ⚠ NADA          |\n");
    fmt::print("      | naoinit       | ⚠ NADA          | ⚠ NADA          |\n");
    fmt::print("      | alinhamento   | ⚠ NADA (GCC)    | ⚠ NADA (GCC)    |\n\n");

    fmt::print("      As mensagens exatas, como saem:\n");
    fmt::print("        heap-buffer-overflow on address 0x502000000010\n");
    fmt::print("        runtime error: signed integer overflow: 2147483647 + 1\n");
    fmt::print("                       cannot be represented in type 'int'\n");
    fmt::print("        runtime error: shift exponent 40 is too large for\n");
    fmt::print("                       32-bit type 'int'\n");
    fmt::print("        runtime error: index 9 out of bounds for type 'int [4]'\n\n");

    fmt::print("      TRÊS resultados que contrariam o senso comum:\n\n");
    fmt::print("      1. `uaf` some com -O1 (explicado em 2.2): o otimizador\n");
    fmt::print("         eliminou a leitura antes da instrumentação.\n\n");
    fmt::print("      2. `alinhamento` NÃO é pego pelo GCC — nem com\n");
    fmt::print("         -fsanitize=alignment isolado. O clang pega:\n");
    fmt::print("           runtime error: load of misaligned address 0x...\n");
    fmt::print("           for type 'int', which requires 4 byte alignment\n");
    fmt::print("         Se alinhamento importa no seu código, use clang para\n");
    fmt::print("         esta checagem.\n\n");
    fmt::print("      3. `aliasing` não é pego por NENHUM dos dois. É a categoria\n");
    fmt::print("         de UB sem rede de proteção — assunto da seção 4.\n");
}

void demo_nao_pega() {
    fmt::print("\n── 3.3 ⚠ O que nenhum sanitizer pega ──\n");

    fmt::print("      (a) STRICT ALIASING. Não há instrumentação para isso; a\n");
    fmt::print("          violação simplesmente produz o valor que o otimizador\n");
    fmt::print("          decidiu. Único recurso: -fno-strict-aliasing (perde\n");
    fmt::print("          otimização) ou não violar (use `bit_cast`).\n\n");

    fmt::print("      (b) LEITURA DE NÃO INICIALIZADO, na prática. O MSan existe,\n");
    fmt::print("          mas exige toda a stdlib instrumentada. Na medição deste\n");
    fmt::print("          subprojeto ele não reportou o caso `naoinit`.\n");
    fmt::print("          A defesa real é o compilador: -Wmaybe-uninitialized\n");
    fmt::print("          (dentro de -Wall) e a regra de sempre inicializar.\n\n");

    fmt::print("      (c) VIOLAÇÃO DE ODR entre unidades de tradução. Só `-flto`\n");
    fmt::print("          com `-Wodr` tem chance. Ver src/linkage/demo_odr.hpp.\n\n");

    fmt::print("      (d) A LÓGICA ERRADA. Nenhum sanitizer sabe que a sua\n");
    fmt::print("          fórmula de desconto está trocada. É para isso que\n");
    fmt::print("          existem testes — src/testing/.\n\n");

    fmt::print("      (e) O CAMINHO QUE NÃO FOI EXECUTADO. Sanitizer é análise\n");
    fmt::print("          DINÂMICA: ele só vê o que rodou. Um `catch` que os\n");
    fmt::print("          testes nunca exercitam pode ter UB e passar limpo.\n");
    fmt::print("          É o que torna sanitizer + cobertura de teste um par:\n");
    fmt::print("          um mede o que rodou, o outro diz o que não rodou.\n");
}

void demo_como_rodar() {
    fmt::print("\n── 3.4 Como rodar ──\n");

    fmt::print("      No compilador:\n");
    fmt::print("        g++ -std=c++20 -O1 -g -fsanitize=address,undefined \\\n");
    fmt::print("            -fno-omit-frame-pointer arq.cpp -o prog\n\n");
    fmt::print("        -g                       → nomes de arquivo e linha\n");
    fmt::print("        -fno-omit-frame-pointer  → pilhas legíveis\n");
    fmt::print("        -O1                      → recomendação oficial do ASan\n");
    fmt::print("        -fno-sanitize-recover=all→ ABORTA no primeiro achado\n");
    fmt::print("                                   (essencial no CI: sem isso, o\n");
    fmt::print("                                    UBSan só imprime e continua,\n");
    fmt::print("                                    e o exit code fica 0)\n\n");

    fmt::print("      No Meson:\n");
    fmt::print("        meson setup build-asan -Db_sanitize=address,undefined \\\n");
    fmt::print("              -Db_lundef=false\n");
    fmt::print("        meson test -C build-asan\n\n");

    fmt::print("      Variáveis de ambiente úteis:\n");
    fmt::print("        ASAN_OPTIONS=detect_leaks=1:abort_on_error=1\n");
    fmt::print("        ASAN_OPTIONS=detect_stack_use_after_return=1\n");
    fmt::print("        UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1\n");
    fmt::print("        TSAN_OPTIONS=second_deadlock_stack=1\n\n");

    fmt::print("      Para excluir falsos positivos de terceiros:\n");
    fmt::print("        ASAN_OPTIONS=suppressions=asan.supp\n");
    fmt::print("        __attribute__((no_sanitize(\"address\"))) void f();\n");
    fmt::print("      ⚠  suprimir deve ser exceção documentada, não hábito.\n\n");

    fmt::print("      Neste subprojeto:\n");
    fmt::print("        g++ -O0 -g -fsanitize=address,undefined \\\n");
    fmt::print("            src/undefined_behavior/ub_exemplos.cpp -o /tmp/ub\n");
    fmt::print("        /tmp/ub               # lista os 12 casos\n");
    fmt::print("        /tmp/ub uaf           # roda um, e veja o diagnóstico\n");
}

inline void run() {
    demo_quais();
    demo_matriz();
    demo_nao_pega();
    demo_como_rodar();
}

}  // namespace demo_sanitizers
