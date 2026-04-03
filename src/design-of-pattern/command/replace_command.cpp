#include "replace_command.hpp"

namespace patterns {

ReplaceCommand::ReplaceCommand(Editor& editor, size_t pos, size_t len, std::string text)
    : editor_(editor), pos_(pos), len_(len), new_text_(std::move(text)) {}

void ReplaceCommand::execute() {
    old_text_ = editor_.content().substr(pos_, len_);
    editor_.erase(pos_, len_);
    editor_.insert(pos_, new_text_);
}

void ReplaceCommand::undo() {
    editor_.erase(pos_, new_text_.size());
    editor_.insert(pos_, old_text_);
}

} // namespace patterns