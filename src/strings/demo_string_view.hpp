// =============================================================================
//  src/strings/demo_string_view.hpp
//
//  std::string_view — a janela que não possui
//  ──────────────────────────────────────────
//  `string_view` é um par (ponteiro, tamanho) sobre caracteres que pertencem a
//  outra pessoa. Ele resolve o problema mais chato das APIs de texto em C++:
//  uma função que aceita `const std::string&` obriga quem tem um `const char*`
//  a construir uma string — alocando — só para passar o argumento.
//
//  Ele já apareceu de raspão em src/stl/ e src/vocabulary_types/. Aqui está o
//  tratamento completo, incluindo as armadilhas que só aparecem em produção.
//
//  Tópicos:
//  • O custo que ele elimina
//  • ⚠ Ele NÃO é null-terminated
//  • ⚠ Ele não possui — as três regras de tempo de vida
//  • ⚠ A conversão implícita que chama strlen
//  • Quando `const std::string&` ainda é a escolha certa
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace demo_string_view {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O custo que ele elimina
// ─────────────────────────────────────────────────────────────────────────────
std::size_t contar_vogais_string(const std::string& s) {
    std::size_t n = 0;
    for (char c : s) if (c=='a'||c=='e'||c=='i'||c=='o'||c=='u') ++n;
    return n;
}

std::size_t contar_vogais_view(std::string_view s) {
    std::size_t n = 0;
    for (char c : s) if (c=='a'||c=='e'||c=='i'||c=='o'||c=='u') ++n;
    return n;
}

void demo_custo() {
    fmt::print("\n── 2.1 O custo que o view elimina ──\n");

    fmt::print("      sizeof(std::string)      = {}\n", sizeof(std::string));
    fmt::print("      sizeof(std::string_view) = {}  (ponteiro + tamanho)\n",
               sizeof(std::string_view));

    const char*       literal = "uma frase razoavelmente longa para o exemplo";
    const std::string str{literal};

    fmt::print("\n      chamando com um `const char*`:\n");
    fmt::print("        contar_vogais_string(literal) → {} vogais\n",
               contar_vogais_string(literal));
    fmt::print("          ⚠  constrói uma std::string TEMPORÁRIA (aloca {} bytes)\n",
               std::strlen(literal) + 1);
    fmt::print("        contar_vogais_view(literal)   → {} vogais\n",
               contar_vogais_view(literal));
    fmt::print("          ✓ nenhuma alocação: só ponteiro + tamanho\n");

    fmt::print("\n      chamando com uma `std::string`: os dois funcionam sem\n");
    fmt::print("      alocar — a string converte para view implicitamente.\n");

    const std::string_view v{str};
    fmt::print("\n      substr sem copiar:\n");
    fmt::print("        str.substr(4, 5)  → '{}'  (aloca uma string nova)\n",
               str.substr(4, 5));
    fmt::print("        v.substr(4, 5)    → '{}'  (só ajusta ponteiro e tamanho)\n",
               v.substr(4, 5));
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ Ele NÃO é null-terminated
//
//     Esta é a diferença que mais causa bug, porque o código compila e funciona
//     nos testes — até a view apontar para o meio de algo.
// ─────────────────────────────────────────────────────────────────────────────
void demo_null_terminator() {
    fmt::print("\n── 2.2 ⚠ string_view não é null-terminated ──\n");

    const std::string texto = "abcdefghij";
    const std::string_view completa{texto};
    const std::string_view parcial = completa.substr(0, 3);   // "abc", sem '\0'

    fmt::print("      texto   = '{}'\n", texto);
    fmt::print("      parcial = '{}' (size={})\n", parcial, parcial.size());

    fmt::print("\n      `parcial.data()` aponta para o 'a' do texto ORIGINAL.\n");
    fmt::print("      Os bytes seguintes são 'd','e','f'... não um '\\0'.\n");

    // Mostra o que uma API C leria — limitado ao buffer conhecido, para não
    // ler fora de limites de verdade.
    fmt::print("      o que `printf(\"%s\", parcial.data())` leria: '{}'\n",
               std::string_view{parcial.data(), texto.size()});
    fmt::print("      ↑ leu ATÉ O FIM da string original, não só os 3 chars.\n");
    fmt::print("        Se a origem não tivesse '\\0', seria leitura fora de\n");
    fmt::print("        limites — e o AddressSanitizer pegaria.\n");

    fmt::print("\n      As três formas de passar um view para uma API C:\n");
    fmt::print("        (a) std::string{{v}}.c_str()   → aloca, mas é correto\n");
    fmt::print("        (b) a variante que aceita (ptr, len):\n");
    fmt::print("              printf(\"%.*s\", (int)v.size(), v.data())\n");
    fmt::print("        (c) garanta na ORIGEM que o view é terminado\n");

    fmt::print("      forma (b) em ação: ");
    std::printf("%.*s\n", static_cast<int>(parcial.size()), parcial.data());

    fmt::print("\n      Regra: `.data()` de um view NUNCA vai direto para uma\n");
    fmt::print("      função que espera C-string.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ As três regras de tempo de vida
// ─────────────────────────────────────────────────────────────────────────────
std::string produz() { return "uma string temporária"; }

class Documento {
public:
    explicit Documento(std::string conteudo) : conteudo_(std::move(conteudo)) {}

    // Seguro: o view aponta para o membro, cuja vida é a do Documento
    std::string_view titulo() const {
        const auto fim = conteudo_.find('\n');
        return std::string_view{conteudo_}.substr(0, fim);
    }

private:
    std::string conteudo_;
};

void demo_lifetime() {
    fmt::print("\n── 2.3 ⚠ Tempo de vida ──\n");

    const Documento doc{"Relatório Anual\nlinha 2\nlinha 3"};
    fmt::print("      doc.titulo() → '{}'  ✓ (aponta para o membro)\n", doc.titulo());

    fmt::print("\n      Os três erros clássicos:\n\n");
    fmt::print("      (a) view de um TEMPORÁRIO:\n");
    fmt::print("            std::string_view v = produz();\n");
    fmt::print("            use(v);   // ← o temporário morreu no ';' acima\n");

    fmt::print("\n      (b) view devolvido apontando para um LOCAL:\n");
    fmt::print("            std::string_view f() {{\n");
    fmt::print("                std::string local = ...;\n");
    fmt::print("                return local;      // ← dangling\n");
    fmt::print("            }}\n");
    fmt::print("            (o GCC avisa: -Wreturn-local-addr)\n");

    fmt::print("\n      (c) view como MEMBRO, com dono não documentado:\n");
    fmt::print("            struct Config {{ std::string_view nome; }};\n");
    fmt::print("            // quem garante que `nome` sobrevive ao Config?\n");

    fmt::print("\n      As três regras:\n");
    fmt::print("        1. view é PARÂMETRO — o uso dominante, sempre seguro\n");
    fmt::print("        2. view como RETORNO só se a origem sobrevive à chamada\n");
    fmt::print("        3. view como MEMBRO exige documentar quem é o dono\n");

    const auto tamanho = [](std::string_view v) { return v.size(); };
    fmt::print("\n      seguro por construção: tamanho(produz()) = {}\n", tamanho(produz()));
    fmt::print("      ↑ o temporário vive até o fim da EXPRESSÃO COMPLETA, e a\n");
    fmt::print("        função terminou de usar o view antes disso.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Conversões
// ─────────────────────────────────────────────────────────────────────────────
void demo_conversoes() {
    fmt::print("\n── 2.4 Conversões ──\n");

    const std::string      s = "texto";
    const std::string_view v = s;              // implícita, barata
    const std::string      t{v};               // EXPLÍCITA — e aloca

    fmt::print("      string → string_view : implícita ✓ (barata)\n");
    fmt::print("      string_view → string : EXPLÍCITA  (custa alocação)\n");
    fmt::print("        `std::string t = v;`  → NÃO COMPILA\n");
    fmt::print("        `std::string t{{v}};`   → compila, e aloca ({} chars)\n", t.size());
    fmt::print("      ↑ intencional: a conversão CARA precisa ser visível.\n");

    fmt::print("\n      Mas há uma conversão implícita que custa:\n");
    fmt::print("        const char* → string_view  chama strlen()\n");
    fmt::print("      Num laço quente com literais, isso é O(n) por iteração.\n");
    fmt::print("      A saída é o literal `sv` (C++17), de tamanho constante:\n");

    using namespace std::string_view_literals;
    constexpr auto literal_sv = "tamanho conhecido na compilação"sv;

    // ⚠ 33, não 31: a frase tem 31 CARACTERES, mas o 'ç' e o 'ã' ocupam 2
    //   bytes cada em UTF-8. `size()` conta bytes — é a mesma lição de
    //   demo_string.hpp, seção 1.5, e aqui ela foi pega por um static_assert.
    static_assert(literal_sv.size() == 33);
    fmt::print("        \"...\"sv.size() = {} (constexpr, sem strlen)\n", literal_sv.size());
    fmt::print("        ⚠  são {} BYTES para 31 caracteres: 'ç' e 'ã' ocupam 2\n",
               literal_sv.size());
    fmt::print("           bytes cada em UTF-8 (ver demo_string.hpp, 1.5)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Quando `const std::string&` ainda é a escolha certa
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando_usar() {
    fmt::print("\n── 2.5 Escolhendo a assinatura ──\n");

    fmt::print("      | assinatura         | quando                          |\n");
    fmt::print("      |--------------------|---------------------------------|\n");
    fmt::print("      | std::string_view   | só LÊ, e não guarda             |\n");
    fmt::print("      | const std::string& | precisa de .c_str() garantido   |\n");
    fmt::print("      | std::string (sink) | vai GUARDAR uma cópia           |\n");
    fmt::print("      | std::string&       | vai MODIFICAR a do chamador     |\n");

    fmt::print("\n      `string_view` é o default para parâmetros de leitura.\n");
    fmt::print("      Prefira `const std::string&` quando:\n");
    fmt::print("        • a função repassa para uma API C que exige '\\0'\n");
    fmt::print("        • a função chama outra que já pede `const string&`\n");
    fmt::print("        • precisa de operações que só `string` tem\n");

    fmt::print("\n      E `std::string` por valor (sink) quando vai guardar —\n");
    fmt::print("      ver src/value_semantics/demo_move_only.hpp, seção 5.3.\n");
    fmt::print("      Um `string_view` ali obrigaria a construir a string dentro\n");
    fmt::print("      da função: o mesmo custo, menos flexível.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_custo();
    demo_null_terminator();
    demo_lifetime();
    demo_conversoes();
    demo_quando_usar();
}

} // namespace demo_string_view
