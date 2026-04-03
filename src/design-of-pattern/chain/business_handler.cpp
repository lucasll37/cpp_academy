#include "business_handler.hpp"
#include <fmt/core.h>

namespace patterns {

void BusinessHandler::handle(const Request& req) {
    fmt::println("[business] processando: \"{}\"", req.body);
    fmt::println("[business] OK — requisição concluída");
}

} // namespace patterns