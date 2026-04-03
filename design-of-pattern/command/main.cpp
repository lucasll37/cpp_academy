#include "editor.hpp"
#include "history.hpp"
#include "insert_command.hpp"
#include "delete_command.hpp"
#include "replace_command.hpp"
#include <fmt/core.h>

using namespace patterns;

static void print(const Editor& e) {
    fmt::println("  buffer: \"{}\"", e.content());
}

int main() {
    Editor  editor;
    History history;

    fmt::println("=== execução ===");
    history.execute(std::make_unique<InsertCommand>(editor, 0, "hello"));
    print(editor);

    history.execute(std::make_unique<InsertCommand>(editor, 5, " world"));
    print(editor);

    history.execute(std::make_unique<ReplaceCommand>(editor, 6, 5, "academy"));
    print(editor);

    history.execute(std::make_unique<DeleteCommand>(editor, 0, 6));
    print(editor);

    fmt::println("\n=== undo x3 ===");
    history.undo(); print(editor);
    history.undo(); print(editor);
    history.undo(); print(editor);

    fmt::println("\n=== redo x2 ===");
    history.redo(); print(editor);
    history.redo(); print(editor);

    fmt::println("\n=== nova ação descarta redo ===");
    history.execute(std::make_unique<InsertCommand>(editor, 0, ">>> "));
    print(editor);
    history.redo(); // não há mais nada para refazer

    return 0;
}