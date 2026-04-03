#include "auth_handler.hpp"
#include <fmt/core.h>

namespace patterns {

void AuthHandler::handle(const Request& req) {
    fmt::println("[auth] verificando token: \"{}\"", req.token);
    if (req.token.empty()) {
        fmt::println("[auth] REJEITADO — token ausente");
        return;
    }
    if (req.token != "valid-token") {
        fmt::println("[auth] REJEITADO — token inválido");
        return;
    }
    fmt::println("[auth] OK");
    pass(req);
}

} // namespace patterns