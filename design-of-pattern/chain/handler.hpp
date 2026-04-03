#pragma once
#include "request.hpp"
#include <memory>

namespace patterns {

struct Handler {
    virtual ~Handler() = default;

    Handler& set_next(std::unique_ptr<Handler> next) {
        next_ = std::move(next);
        return *next_;
    }

    virtual void handle(const Request& req) = 0;

protected:
    void pass(const Request& req) {
        if (next_) next_->handle(req);
    }

private:
    std::unique_ptr<Handler> next_;
};

} // namespace patterns