#include "rate_limit_handler.hpp"
#include <fmt/core.h>

namespace patterns {

RateLimitHandler::RateLimitHandler(int max_calls) : max_calls_(max_calls) {}

void RateLimitHandler::handle(const Request& req) {
    fmt::println("[rate_limit] {}/{} chamadas por minuto", req.calls_per_minute, max_calls_);
    if (req.calls_per_minute > max_calls_) {
        fmt::println("[rate_limit] REJEITADO — limite excedido");
        return;
    }
    fmt::println("[rate_limit] OK");
    pass(req);
}

} // namespace patterns