#pragma once
#include "handler.hpp"
#include "request.hpp"

namespace patterns {

struct ValidationHandler : Handler {
    void handle(const Request& req) override;
};

} // namespace patterns