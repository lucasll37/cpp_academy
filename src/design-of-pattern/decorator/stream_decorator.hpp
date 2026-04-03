#pragma once
#include "data_stream.hpp"
#include <memory>

namespace patterns {

struct StreamDecorator : DataStream {
    explicit StreamDecorator(std::unique_ptr<DataStream> wrapped)
        : wrapped_(std::move(wrapped)) {}

    void write(const std::string& data) override {
        wrapped_->write(data);
    }

    std::string read() override {
        return wrapped_->read();
    }

protected:
    std::unique_ptr<DataStream> wrapped_;
};

} // namespace patterns