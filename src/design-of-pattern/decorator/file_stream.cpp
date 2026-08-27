#include "file_stream.hpp"

namespace patterns {

void FileStream::write(const std::string& data) {
    buffer_ = data;
}

std::string FileStream::read() {
    return buffer_;
}

} // namespace patterns