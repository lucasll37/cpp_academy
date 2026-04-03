#include "delete_command.hpp"

namespace patterns {

DeleteCommand::DeleteCommand(Editor& editor, size_t pos, size_t len)
    : editor_(editor), pos_(pos), len_(len) {}

void DeleteCommand::execute() {
    deleted_ = editor_.content().substr(pos_, len_);
    editor_.erase(pos_, len_);
}

void DeleteCommand::undo() {
    editor_.insert(pos_, deleted_);
}

} // namespace patterns