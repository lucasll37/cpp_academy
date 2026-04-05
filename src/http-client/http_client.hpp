#pragma once

#include <string>
#include <map>
#include <stdexcept>

namespace http {

using Headers = std::map<std::string, std::string>;

struct Response {
    long        status_code;
    std::string body;
    Headers     headers;
};

struct HttpError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// GET síncrono. Lança HttpError em caso de falha de transporte.
// Erros HTTP (4xx, 5xx) são retornados normalmente no status_code.
Response get(const std::string& url,
             const Headers&     request_headers = {});

} // namespace http
