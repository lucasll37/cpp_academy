#pragma once
#include <string>

namespace patterns {

struct Request {
    std::string token;
    std::string body;
    int         calls_per_minute;
};

} // namespace patterns