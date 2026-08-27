#include "http_client.hpp"

#include <curl/curl.h>

#include <sstream>
#include <algorithm>
#include <cctype>

namespace http {

// ─── RAII ────────────────────────────────────────────────────────────────────

struct CurlHandle {
    CURL* h;

    CurlHandle() : h(curl_easy_init()) {
        if (!h) throw HttpError("curl_easy_init failed");
    }
    ~CurlHandle() { curl_easy_cleanup(h); }

    // não copiável
    CurlHandle(const CurlHandle&)            = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;
};

struct CurlSlist {
    curl_slist* list = nullptr;

    void append(const std::string& header) {
        list = curl_slist_append(list, header.c_str());
    }
    ~CurlSlist() { if (list) curl_slist_free_all(list); }
};

// ─── Callbacks ───────────────────────────────────────────────────────────────

// Acumula chunks do body em uma std::string
static size_t write_body(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// Parseia cada linha de header recebida (formato "Name: Value\r\n")
static size_t write_header(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* headers = static_cast<Headers*>(userdata);
    std::string line(ptr, size * nmemb);

    auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // Trim whitespace e \r\n
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                [](unsigned char c){ return !std::isspace(c); }));
            s.erase(std::find_if(s.rbegin(), s.rend(),
                [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
        };
        trim(key);
        trim(value);

        // Lowercase na chave — convenção HTTP/1.1
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        (*headers)[key] = value;
    }

    return size * nmemb;
}

// ─── Public API ──────────────────────────────────────────────────────────────

Response get(const std::string& url, const Headers& request_headers) {
    CurlHandle curl;

    std::string body;
    Headers     resp_headers;

    // URL
    curl_easy_setopt(curl.h, CURLOPT_URL, url.c_str());

    // Segue redirecionamentos (301, 302...)
    curl_easy_setopt(curl.h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.h, CURLOPT_MAXREDIRS, 5L);

    // Timeout
    curl_easy_setopt(curl.h, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl.h, CURLOPT_CONNECTTIMEOUT, 10L);

    // Verifica certificado TLS
    curl_easy_setopt(curl.h, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.h, CURLOPT_SSL_VERIFYHOST, 2L);

    // Callbacks de escrita
    curl_easy_setopt(curl.h, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl.h, CURLOPT_WRITEDATA,     &body);

    curl_easy_setopt(curl.h, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl.h, CURLOPT_HEADERDATA,     &resp_headers);

    // Headers customizados
    CurlSlist slist;
    slist.append("Accept: application/json");
    for (auto& [k, v] : request_headers)
        slist.append(k + ": " + v);
    curl_easy_setopt(curl.h, CURLOPT_HTTPHEADER, slist.list);

    // Executa a requisição
    CURLcode res = curl_easy_perform(curl.h);
    if (res != CURLE_OK)
        throw HttpError(curl_easy_strerror(res));

    long status = 0;
    curl_easy_getinfo(curl.h, CURLINFO_RESPONSE_CODE, &status);

    return {status, std::move(body), std::move(resp_headers)};
}

} // namespace http
