#include "history.hpp"
#include <fmt/core.h>

namespace patterns {

void History::execute(std::unique_ptr<Command> cmd) {
    cmd->execute();
    done_.push_back(std::move(cmd));
    undone_.clear(); // nova ação descarta o histórico de redo
}

void History::undo() {
    if (done_.empty()) {
        fmt::println("  [history] nada para desfazer");
        return;
    }
    done_.back()->undo();
    undone_.push_back(std::move(done_.back()));
    done_.pop_back();
}

void History::redo() {
    if (undone_.empty()) {
        fmt::println("  [history] nada para refazer");
        return;
    }
    undone_.back()->execute();
    done_.push_back(std::move(undone_.back()));
    undone_.pop_back();
}

} // namespace patterns