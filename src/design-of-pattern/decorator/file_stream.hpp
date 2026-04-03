#pragma once
#include "data_stream.hpp"

namespace patterns {

struct FileStream : DataStream {
    void write(const std::string& data) override;
    std::string read() override;
private:
    std::string buffer_;
};

} // namespace patterns