// =============================================================================
//  src/design-of-pattern/memento/main.cpp
//
//  MEMENTO — salvar e restaurar SEM violar o encapsulamento
//  ────────────────────────────────────────────────────────
//  A parte que quase todo mundo perde é a segunda metade da frase. Um
//  "memento" com campos públicos não é um Memento — é um struct de dados, e
//  ele acopla quem guarda ao INTERNO do objeto salvo.
//
//  Em C++, a opacidade se implementa com membros privados + `friend`.
// =============================================================================
#include "editor.hpp"
#include "historico.hpp"

#include <fmt/core.h>

#include <string>
#include <vector>

static void secao(const char* titulo) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", titulo);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

static void mostrar(const memento::Editor& e, const char* rotulo) {
    fmt::print("    {:<24} '{}' (cursor={})\n", rotulo, e.conteudo(), e.cursor());
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│              cpp_academy — Memento (GoF)                 │\n");
    fmt::print("│   \"Capture e externalize o estado interno de um objeto,  │\n");
    fmt::print("│    sem violar o encapsulamento.\"                — GoF    │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. Os três papéis");

    fmt::print("  ORIGINADOR (Editor)     cria e restaura mementos;\n");
    fmt::print("                          é o ÚNICO que consegue lê-los\n");
    fmt::print("  MEMENTO    (Estado)     opaco para todos, exceto o originador\n");
    fmt::print("  ZELADOR    (Historico)  GUARDA os mementos, nunca os inspeciona\n\n");

    fmt::print("  Em C++, a opacidade é:\n");
    fmt::print("    class Editor {{\n");
    fmt::print("        class Estado {{\n");
    fmt::print("            std::string conteudo_;      // privado\n");
    fmt::print("            std::size_t cursor_;        // privado\n");
    fmt::print("            Estado(...);                // ctor PRIVADO\n");
    fmt::print("            friend class Editor;        // ← só ele\n");
    fmt::print("        }};\n");
    fmt::print("    }};\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. Desfazer e refazer");

    memento::Editor    editor;
    memento::Historico historico;

    const auto editar = [&](const char* rotulo, auto&& acao) {
        historico.registrar(editor.salvar(rotulo));   // salva ANTES de mudar
        acao();
        mostrar(editor, rotulo);
    };

    mostrar(editor, "inicial");
    editar("escrever 'Ola'",     [&] { editor.inserir("Ola"); });
    editar("escrever ' mundo'",  [&] { editor.inserir(" mundo"); });
    editar("escrever '!'",       [&] { editor.inserir("!"); });
    editar("apagar 6",           [&] { editor.apagar(6); });

    fmt::print("\n  desfazendo três vezes:\n");
    for (int i = 0; i < 3; ++i) {
        historico.desfazer(editor);
        mostrar(editor, fmt::format("desfazer #{}", i + 1).c_str());
    }

    fmt::print("\n  refazendo duas vezes:\n");
    for (int i = 0; i < 2; ++i) {
        historico.refazer(editor);
        mostrar(editor, fmt::format("refazer #{}", i + 1).c_str());
    }

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. O que o zelador NÃO consegue fazer");

    fmt::print("  O `Historico` guarda `Editor::Estado`, mas:\n\n");
    fmt::print("    Editor::Estado e = editor.salvar(\"x\");\n");
    fmt::print("    e.conteudo_;                    // ✗ 'conteudo_' is private\n");
    fmt::print("    e.cursor_ = 0;                  // ✗ 'cursor_' is private\n");
    fmt::print("    Editor::Estado{{\"txt\", 0, \"r\"}};  // ✗ ctor is private\n\n");

    fmt::print("  O que ele PODE, e é tudo que precisa:\n");
    fmt::print("    • guardar num vector\n");
    fmt::print("    • devolver ao editor via `restaurar()`\n");
    fmt::print("    • ler os METADADOS públicos (rótulo, tamanho)\n\n");

    fmt::print("  os rótulos que o histórico consegue exibir:\n");
    for (const auto& r : historico.rotulos()) fmt::print("    {}\n", r);

    fmt::print("\n  Repare no que isso permite: o `Editor` pode TROCAR a\n");
    fmt::print("  representação interna — de `std::string` para um rope, para\n");
    fmt::print("  um gap buffer — e o `Historico` não muda uma linha. Ele nunca\n");
    fmt::print("  soube o que havia lá dentro.\n");
    fmt::print("  É o encapsulamento fazendo o trabalho dele.\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. O custo: memória");

    fmt::print("  o histórico atual:\n");
    fmt::print("    {} estados guardados, {} bytes de conteúdo\n",
               historico.tamanho(), historico.bytes());

    fmt::print("\n  ⚠  cada memento é uma CÓPIA COMPLETA do estado. Num editor\n");
    fmt::print("     real, com um documento de 10 MB e 100 níveis de undo, isso\n");
    fmt::print("     seria 1 GB.\n\n");

    fmt::print("  As três saídas, e a que os editores reais usam:\n\n");
    fmt::print("    (a) LIMITAR o histórico\n");
    fmt::print("        → simples, e é o que este demo oferece:\n");
    historico.definir_limite(2);
    fmt::print("          após definir_limite(2): {} estados\n", historico.tamanho());

    fmt::print("\n    (b) guardar o DELTA em vez do estado completo\n");
    fmt::print("        → \"inseriu 'abc' na posição 5\" em vez do documento todo\n");
    fmt::print("        → é o padrão COMMAND, não o Memento\n");
    fmt::print("          (design-of-pattern/command/)\n");

    fmt::print("\n    (c) estruturas PERSISTENTES (copy-on-write)\n");
    fmt::print("        → o \"snapshot\" compartilha os nós não modificados\n");
    fmt::print("        → é o que ropes e árvores persistentes fazem\n\n");

    fmt::print("  Editores de texto sérios usam (b) + (c): comandos reversíveis\n");
    fmt::print("  sobre uma estrutura persistente. O Memento puro só se paga\n");
    fmt::print("  quando o estado é PEQUENO ou o histórico é CURTO.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. Memento vs Command vs Prototype");

    fmt::print("  Os três lidam com \"guardar para usar depois\":\n\n");
    fmt::print("  | padrão    | guarda            | desfazer é      | custo    |\n");
    fmt::print("  |-----------|-------------------|-----------------|----------|\n");
    fmt::print("  | Memento   | o ESTADO inteiro  | restaurar       | O(estado)|\n");
    fmt::print("  | Command   | a OPERAÇÃO        | operação inversa| O(delta) |\n");
    fmt::print("  | Prototype | uma CÓPIA pública | —               | O(estado)|\n\n");

    fmt::print("  Memento vs Prototype é a distinção mais sutil:\n");
    fmt::print("    Prototype → o clone é um objeto NORMAL, usável, público\n");
    fmt::print("    Memento   → o snapshot é OPACO, e só serve para restaurar\n");
    fmt::print("  Um `clonar()` público expõe a capacidade de copiar; um\n");
    fmt::print("  `salvar()` que devolve um tipo opaco, não.\n");
    fmt::print("  Ver design-of-pattern/prototype/.\n\n");

    fmt::print("  Memento vs Command:\n");
    fmt::print("    \"a operação tem inversa BARATA de calcular?\"\n");
    fmt::print("      sim → Command (guarde a operação)\n");
    fmt::print("      não → Memento (guarde o estado)\n");
    fmt::print("  Inserir texto tem inversa óbvia (apagar). Um filtro de imagem\n");
    fmt::print("  destrutivo não tem — aí o Memento é a única saída.\n\n");

    fmt::print("  ⚠  e eles se COMBINAM: um Command que não consegue se\n");
    fmt::print("     inverter guarda um Memento do estado anterior. É assim que\n");
    fmt::print("     editores de imagem fazem com filtros destrutivos.\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║                Resumo do Memento                         ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : salvar/restaurar SEM violar encapsulamento  ║\n");
    fmt::print("║ Papéis     : originador, memento (opaco), zelador        ║\n");
    fmt::print("║ Em C++     : membros privados + friend do originador     ║\n");
    fmt::print("║ Ganho      : o originador pode TROCAR a representação    ║\n");
    fmt::print("║ Custo      : cópia completa do estado, por snapshot      ║\n");
    fmt::print("║ Mitigações : limitar histórico, delta (Command), COW     ║\n");
    fmt::print("║ vs Command : Memento guarda ESTADO; Command, a OPERAÇÃO  ║\n");
    fmt::print("║ vs Prototype: o clone é público e usável; o memento, não ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ memento — todos os demos concluídos\n\n");

    return 0;
}
