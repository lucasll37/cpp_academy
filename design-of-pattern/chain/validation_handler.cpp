#include "validation_handler.hpp"
#include <fmt/core.h>

namespace patterns {

void ValidationHandler::handle(const Request& req) {
    fmt::println("[validation] verificando body: \"{}\"", req.body);
    if (req.body.empty()) {
        fmt::println("[validation] REJEITADO — body vazio");
        return;
    }
    if (req.body.size() > 128) {
        fmt::println("[validation] REJEITADO — body excede 128 chars");
        return;
    }
    fmt::println("[validation] OK");
    pass(req);
}

} // namespace patterns