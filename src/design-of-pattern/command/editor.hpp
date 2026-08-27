#pragma once
#include <string>

namespace patterns {

struct Editor {
    void insert(size_t pos, const std::string& text);
    void erase(size_t pos, size_t len);
    const std::string& content() const;
private:
    std::string buffer_;
};

} // namespace patterns