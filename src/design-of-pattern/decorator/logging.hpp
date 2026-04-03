#pragma once
#include "stream_decorator.hpp"

namespace patterns {

struct Logging : StreamDecorator {
    using StreamDecorator::StreamDecorator;
    void write(const std::string& data) override;
    std::string read() override;
};

} // namespace patterns