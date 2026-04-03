#pragma once
#include "command.hpp"
#include <memory>
#include <vector>

namespace patterns {

struct History {
    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
private:
    std::vector<std::unique_ptr<Command>> done_;
    std::vector<std::unique_ptr<Command>> undone_;
};

} // namespace patterns