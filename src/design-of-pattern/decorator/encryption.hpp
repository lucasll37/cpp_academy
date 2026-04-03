#pragma once
#include "stream_decorator.hpp"

namespace patterns {

struct Encryption : StreamDecorator {
    using StreamDecorator::StreamDecorator;
    void write(const std::string& data) override;
    std::string read() override;
};

} // namespace patterns