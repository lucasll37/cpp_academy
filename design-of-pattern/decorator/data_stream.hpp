#pragma once
#include <string>

namespace patterns {

struct DataStream {
    virtual ~DataStream() = default;
    virtual void write(const std::string& data) = 0;
    virtual std::string read() = 0;
};

} // namespace patterns