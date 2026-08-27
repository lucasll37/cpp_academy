#pragma once
#include "handler.hpp"
#include "request.hpp"

namespace patterns {

struct AuthHandler : Handler {
    void handle(const Request& req) override;
};

} // namespace patterns