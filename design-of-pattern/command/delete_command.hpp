#pragma once
#include "command.hpp"
#include "editor.hpp"
#include <string>

namespace patterns {

struct DeleteCommand : Command {
    DeleteCommand(Editor& editor, size_t pos, size_t len);
    void execute() override;
    void undo() override;
private:
    Editor&     editor_;
    size_t      pos_;
    size_t      len_;
    std::string deleted_; // salvo no execute para poder restaurar no undo
};

} // namespace patterns