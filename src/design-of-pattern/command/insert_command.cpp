#include "insert_command.hpp"

namespace patterns {

InsertCommand::InsertCommand(Editor& editor, size_t pos, std::string text)
    : editor_(editor), pos_(pos), text_(std::move(text)) {}

void InsertCommand::execute() {
    editor_.insert(pos_, text_);
}

void InsertCommand::undo() {
    editor_.erase(pos_, text_.size());
}

} // namespace patterns