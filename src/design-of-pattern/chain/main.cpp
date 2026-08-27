#include "auth_handler.hpp"
#include "rate_limit_handler.hpp"
#include "validation_handler.hpp"
#include "business_handler.hpp"
#include <fmt/core.h>
#include <memory>

using namespace patterns;

static void run(const char* label, const Request& req) {
    fmt::println("\n=== {} ===", label);

    auto auth  = std::make_unique<AuthHandler>();
    auto& rate = auth->set_next(std::make_unique<RateLimitHandler>(60));
    auto& val  = rate.set_next(std::make_unique<ValidationHandler>());
    val.set_next(std::make_unique<BusinessHandler>());

    auth->handle(req);
}

int main() {
    run("token inválido",    {"wrong-token", "payload ok",   10});
    run("rate limit",        {"valid-token", "payload ok",  100});
    run("body vazio",        {"valid-token", "",              10});
    run("requisição válida", {"valid-token", "payload ok",   10});

    return 0;
}