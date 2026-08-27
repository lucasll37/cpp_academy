#include "editor.hpp"

namespace patterns {

void Editor::insert(size_t pos, const std::string& text) {
    buffer_.insert(pos, text);
}

void Editor::erase(size_t pos, size_t len) {
    buffer_.erase(pos, len);
}

const std::string& Editor::content() const {
    return buffer_;
}

} // namespace patterns