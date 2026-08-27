#pragma once
#include "command.hpp"
#include "editor.hpp"
#include <string>

namespace patterns {

struct ReplaceCommand : Command {
    ReplaceCommand(Editor& editor, size_t pos, size_t len, std::string text);
    void execute() override;
    void undo() override;
private:
    Editor&     editor_;
    size_t      pos_;
    size_t      len_;
    std::string new_text_;
    std::string old_text_; // salvo no execute para restaurar no undo
};

} // namespace patterns