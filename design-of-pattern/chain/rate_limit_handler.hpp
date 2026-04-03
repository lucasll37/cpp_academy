#pragma once
#include "handler.hpp"
#include "request.hpp"

namespace patterns {

struct RateLimitHandler : Handler {
    explicit RateLimitHandler(int max_calls);
    void handle(const Request& req) override;
private:
    int max_calls_;
};

} // namespace patterns