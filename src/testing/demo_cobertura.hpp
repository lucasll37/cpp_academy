// =============================================================================
//  src/testing/demo_cobertura.hpp
//
//  Cobertura, CI e o que os testes NÃO pegam
//  ─────────────────────────────────────────
//  Cobertura mede quais linhas foram EXECUTADAS. Ela não mede se o
//  comportamento estava certo — um teste sem nenhuma asserção dá 100%.
//  É por isso que "cobertura como meta" é um dos incentivos mais perversos
//  que uma equipe pode adotar.
//
//  Tópicos:
//  • Como medir de verdade (gcov/lcov)
//  • Os quatro tipos de cobertura
//  • ⚠ O que 100% de cobertura NÃO garante
//  • Testes sob sanitizer — a combinação que vale mais
//  • Teste baseado em propriedades e fuzzing
// =============================================================================
#pragma once
#include <fmt/core.h>

namespace demo_cobertura {

void demo_como_medir() {
    fmt::print("\n── 5.1 Como medir ──\n");

    fmt::print("      Com GCC/gcov:\n");
    fmt::print("        g++ --coverage -O0 -g fonte.cpp teste.cpp -lgtest_main -lgtest\n");
    fmt::print("        ./a.out                       # gera .gcda\n");
    fmt::print("        gcov fonte.cpp                # relatório por arquivo\n");
    fmt::print("        lcov --capture -d . -o cov.info\n");
    fmt::print("        genhtml cov.info -o html/     # relatório navegável\n\n");

    fmt::print("      Com Clang (mais preciso, e mostra a cobertura de REGIÃO):\n");
    fmt::print("        clang++ -fprofile-instr-generate -fcoverage-mapping ...\n");
    fmt::print("        LLVM_PROFILE_FILE=t.profraw ./a.out\n");
    fmt::print("        llvm-profdata merge t.profraw -o t.profdata\n");
    fmt::print("        llvm-cov show ./a.out -instr-profile=t.profdata\n\n");

    fmt::print("      Pelo Meson:\n");
    fmt::print("        meson setup build -Db_coverage=true\n");
    fmt::print("        meson test -C build\n");
    fmt::print("        ninja -C build coverage-html\n\n");

    fmt::print("      ⚠  Compile com -O0. Com otimização, as linhas do relatório\n");
    fmt::print("         não correspondem mais ao fonte — inlining e eliminação\n");
    fmt::print("         de código morto embaralham tudo.\n");
}

void demo_tipos() {
    fmt::print("\n── 5.2 Os quatro tipos de cobertura ──\n");

    fmt::print("      Considere:\n");
    fmt::print("        if (a && b) faz_algo();\n\n");

    fmt::print("      LINHA      — a linha foi executada?\n");
    fmt::print("                   1 teste com a=true, b=true já dá 100%.\n\n");
    fmt::print("      RAMO       — os dois lados do `if` foram tomados?\n");
    fmt::print("                   precisa de 2 testes.\n\n");
    fmt::print("      CONDIÇÃO   — cada subcondição foi true E false?\n");
    fmt::print("                   `a` e `b` precisam variar independentemente.\n\n");
    fmt::print("      CAMINHO    — todas as combinações de todos os ramos da\n");
    fmt::print("                   função. Cresce exponencialmente; inviável\n");
    fmt::print("                   além de funções pequenas.\n\n");

    fmt::print("      A maioria das ferramentas reporta LINHA por padrão — a\n");
    fmt::print("      métrica mais fácil e a menos informativa. Peça RAMO:\n");
    fmt::print("        gcov -b fonte.cpp\n");
    fmt::print("        lcov --rc branch_coverage=1 ...\n\n");

    fmt::print("      O curto-circuito do `&&` é o que separa linha de condição:\n");
    fmt::print("      com a=false, `b` nunca é AVALIADO — a linha conta como\n");
    fmt::print("      coberta, e metade da lógica nunca rodou.\n");
}

void demo_limites() {
    fmt::print("\n── 5.3 ⚠ O que 100% NÃO garante ──\n");

    fmt::print("      (a) que as ASSERÇÕES existem. Este teste dá 100% de\n");
    fmt::print("          cobertura da função e não verifica NADA:\n");
    fmt::print("            TEST(X, Cobre) {{ minha_funcao(1, 2); }}\n\n");

    fmt::print("      (b) que os VALORES certos foram testados. Cobrir\n");
    fmt::print("          `if (n >= 10)` com n=5 e n=50 é 100% de ramo — e não\n");
    fmt::print("          descobre que o certo era `> 10`. As FRONTEIRAS (9, 10)\n");
    fmt::print("          é que encontram esse bug, e cobertura não pede por elas.\n\n");

    fmt::print("      (c) que o código que FALTA existe. Um `else` esquecido não\n");
    fmt::print("          aparece em relatório nenhum: cobertura mede o que está\n");
    fmt::print("          escrito, não o que deveria estar.\n\n");

    fmt::print("      (d) ausência de UB. Um `int` que estoura executa a linha e\n");
    fmt::print("          conta como coberto.\n\n");

    fmt::print("      Use cobertura como um DETECTOR DE BURACOS, lendo o\n");
    fmt::print("      relatório: \"este `catch` nunca rodou\", \"esta função inteira\n");
    fmt::print("      está em zero\". Como META numérica, ela produz testes sem\n");
    fmt::print("      asserção escritos para satisfazer o gate do CI.\n\n");

    fmt::print("      A métrica melhor, se você quiser uma: TESTE DE MUTAÇÃO.\n");
    fmt::print("      A ferramenta altera o seu código (troca `>=` por `>`,\n");
    fmt::print("      inverte um `if`) e verifica se ALGUM teste falha. Um mutante\n");
    fmt::print("      que sobrevive é um buraco REAL. Em C++: `mull`, `Dextool`.\n");
}

void demo_sanitizers() {
    fmt::print("\n── 5.4 Testes sob sanitizer ──\n");

    fmt::print("      Esta é a combinação de melhor retorno em C++:\n\n");
    fmt::print("        meson setup build-asan -Db_sanitize=address,undefined\n");
    fmt::print("        meson test -C build-asan\n\n");

    fmt::print("      Os testes já EXERCITAM o código. Rodá-los sob sanitizer\n");
    fmt::print("      transforma cada um num detector de use-after-free,\n");
    fmt::print("      buffer overflow, leak e UB aritmético — sem escrever um\n");
    fmt::print("      teste a mais.\n\n");

    fmt::print("      | sanitizer   | pega                        | custo   |\n");
    fmt::print("      |-------------|-----------------------------|---------|\n");
    fmt::print("      | address     | UAF, overflow, leak         | ~2×     |\n");
    fmt::print("      | undefined   | overflow, shift, alinhamento| ~1.2×   |\n");
    fmt::print("      | thread      | data races                  | ~5-15×  |\n");
    fmt::print("      | memory      | leitura de não-inicializado | ~3×     |\n\n");

    fmt::print("      ⚠  `address` e `thread` são INCOMPATÍVEIS entre si — dois\n");
    fmt::print("         jobs separados no CI. `memory` exige que TODA a pilha,\n");
    fmt::print("         inclusive a libstdc++, esteja instrumentada, o que na\n");
    fmt::print("         prática limita seu uso.\n\n");

    fmt::print("      Todo subprojeto deste repositório é validado assim. Ver\n");
    fmt::print("      src/undefined_behavior/ para o que cada um pega, com\n");
    fmt::print("      exemplos que estouram de verdade.\n");
}

void demo_alem() {
    fmt::print("\n── 5.5 Além dos testes de exemplo ──\n");

    fmt::print("      Um teste de exemplo verifica UM caso que você imaginou. As\n");
    fmt::print("      duas técnicas abaixo procuram os casos que você NÃO imaginou.\n\n");

    fmt::print("      TESTE BASEADO EM PROPRIEDADES — em vez de\n");
    fmt::print("      \"f(2,3) == 5\", afirme uma PROPRIEDADE que vale sempre, e\n");
    fmt::print("      deixe a ferramenta gerar centenas de entradas:\n\n");
    fmt::print("        • idempotência:  ordena(ordena(v)) == ordena(v)\n");
    fmt::print("        • ida e volta:   desserializa(serializa(x)) == x\n");
    fmt::print("        • invariante:    tamanho(v) não muda ao ordenar\n");
    fmt::print("        • oráculo:       minha_versao(x) == versao_lenta(x)\n\n");
    fmt::print("      No carrinho: \"o total nunca é maior que o subtotal\" e\n");
    fmt::print("      \"acrescentar um item nunca diminui o subtotal\".\n");
    fmt::print("      Bibliotecas: RapidCheck, rapidcheck-gtest.\n\n");

    fmt::print("      FUZZING — alimenta a função com entradas geradas e guiadas\n");
    fmt::print("      por COBERTURA, procurando crash e UB:\n\n");
    fmt::print("        // fuzz.cpp\n");
    fmt::print("        extern \"C\" int LLVMFuzzerTestOneInput(\n");
    fmt::print("                const uint8_t* d, size_t n) {{\n");
    fmt::print("            meu_parser(std::string_view{{\n");
    fmt::print("                reinterpret_cast<const char*>(d), n}});\n");
    fmt::print("            return 0;\n");
    fmt::print("        }}\n");
    fmt::print("        clang++ -fsanitize=fuzzer,address fuzz.cpp\n\n");
    fmt::print("      Vale a pena para tudo que recebe entrada NÃO CONFIÁVEL:\n");
    fmt::print("      parsers, decodificadores, protocolos de rede.\n");
    fmt::print("      O `extern \"C\"` ali não é decoração — ver src/linkage/.\n");
}

inline void run() {
    demo_como_medir();
    demo_tipos();
    demo_limites();
    demo_sanitizers();
    demo_alem();
}

}  // namespace demo_cobertura
