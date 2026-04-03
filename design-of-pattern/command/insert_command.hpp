#pragma once
#include "command.hpp"
#include "editor.hpp"
#include <string>

namespace patterns {

struct InsertCommand : Command {
    InsertCommand(Editor& editor, size_t pos, std::string text);
    void execute() override;
    void undo() override;
private:
    Editor&     editor_;
    size_t      pos_;
    std::string text_;
};

} // namespace patterns