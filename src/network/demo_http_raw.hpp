// =============================================================================
//  src/network/demo_http_raw.hpp
//
//  HTTP sobre TCP Raw com Asio standalone
//  ────────────────────────────────────────
//  HTTP/1.1 é texto sobre TCP. Entender isso desmistifica o protocolo —
//  é simplesmente uma conversa de strings com formato específico.
//
//  Request:
//      METHOD /path HTTP/1.1\r\n
//      Host: hostname\r\n
//      Header: valor\r\n
//      \r\n                    ← linha vazia = fim dos headers
//      [body]                  ← opcional, para POST/PUT
//
//  Response:
//      HTTP/1.1 STATUS_CODE STATUS_TEXT\r\n
//      Header: valor\r\n
//      \r\n
//      [body]
//
//  Nota: os demos que fazem request externo (httpbin.org) requerem
//  conectividade. Os demos locais funcionam sem internet.
//
//  Tópicos:
//  • Construção manual de request HTTP/1.1
//  • Parsing de status line + headers + body
//  • GET síncrono
//  • GET assíncrono com callback chain completa
//  • Comparativo: TCP raw vs libcurl
// =============================================================================
#pragma once

#define ASIO_STANDALONE
#include <asio.hpp>

#include <fmt/core.h>
#include <string>
#include <sstream>

namespace demo_http_raw {

using tcp = asio::ip::tcp;

// ─────────────────────────────────────────────────────────────────────────────
//  Estrutura de resposta HTTP parseada
// ─────────────────────────────────────────────────────────────────────────────
struct HttpResponse {
    int         status  = 0;
    std::string status_text;
    std::string headers;
    std::string body;
    bool ok() const { return status >= 200 && status < 300; }
};

HttpResponse parsear(const std::string& raw) {
    HttpResponse r;
    std::istringstream s(raw);
    std::string linha;

    // "HTTP/1.1 200 OK"
    if (std::getline(s, linha)) {
        std::istringstream sl(linha);
        std::string ver;
        sl >> ver >> r.status;
        std::getline(sl, r.status_text);
        if (!r.status_text.empty() && r.status_text.front() == ' ')
            r.status_text = r.status_text.substr(1);
        if (!r.status_text.empty() && r.status_text.back() == '\r')
            r.status_text.pop_back();
    }

    // Headers até linha vazia
    while (std::getline(s, linha) && linha != "\r" && !linha.empty()) {
        if (!linha.empty() && linha.back() == '\r') linha.pop_back();
        r.headers += linha + "\n";
    }

    // Body
    std::ostringstream b;
    b << s.rdbuf();
    r.body = b.str();
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. GET síncrono — HTTP nu e cru
// ─────────────────────────────────────────────────────────────────────────────
void demo_get_sincrono() {
    fmt::print("\n── 5.1 HTTP GET síncrono (httpbin.org) ──\n");

    try {
        asio::io_context io;
        tcp::resolver resolver(io);
        tcp::socket socket(io);

        asio::error_code ec;
        auto endpoints = resolver.resolve("httpbin.org", "80", ec);
        if (ec) {
            fmt::print("  sem conectividade: {}\n", ec.message());
            return;
        }

        asio::connect(socket, endpoints, ec);
        if (ec) { fmt::print("  connect: {}\n", ec.message()); return; }

        // HTTP/1.1 request — é só texto formatado!
        std::string req =
            "GET /get HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "User-Agent: cpp_academy/1.0\r\n"
            "Accept: application/json\r\n"
            "Connection: close\r\n"
            "\r\n";

        fmt::print("  request enviado:\n");
        fmt::print("    GET /get HTTP/1.1\n");
        fmt::print("    Host: httpbin.org\n");
        fmt::print("    Connection: close\n");

        asio::write(socket, asio::buffer(req));

        // Lê até EOF (Connection: close sinaliza fim)
        asio::streambuf buf;
        asio::read(socket, buf, asio::transfer_all(), ec);
        if (ec && ec != asio::error::eof) {
            fmt::print("  read: {}\n", ec.message());
            return;
        }

        std::string raw(asio::buffers_begin(buf.data()),
                        asio::buffers_end(buf.data()));
        auto resp = parsear(raw);

        fmt::print("  status: {} {}\n", resp.status, resp.status_text);
        fmt::print("  body (200 chars):\n    {}\n",
                   resp.body.substr(0, std::min<size_t>(200, resp.body.size())));

    } catch (const std::exception& e) {
        fmt::print("  exceção: {}\n", e.what());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. GET assíncrono — callback chain completa
//     resolve → connect → write → read → parse
// ─────────────────────────────────────────────────────────────────────────────
class HttpGetAsync {
public:
    HttpGetAsync(asio::io_context& io, const std::string& host,
                 const std::string& path)
        : socket_(io), resolver_(io), host_(host)
    {
        req_ = "GET " + path + " HTTP/1.1\r\n"
               "Host: " + host + "\r\n"
               "User-Agent: cpp_academy/1.0\r\n"
               "Connection: close\r\n\r\n";
    }

    void iniciar() {
        resolver_.async_resolve(host_, "80",
            [this](asio::error_code ec, tcp::resolver::results_type eps) {
                if (ec) { fmt::print("  [async] resolve: {}\n", ec.message()); return; }
                asio::async_connect(socket_, eps,
                    [this](asio::error_code ec, const tcp::endpoint&) {
                        if (ec) { fmt::print("  [async] connect: {}\n", ec.message()); return; }
                        asio::async_write(socket_, asio::buffer(req_),
                            [this](asio::error_code ec, std::size_t) {
                                if (ec) { fmt::print("  [async] write: {}\n", ec.message()); return; }
                                asio::async_read(socket_, buf_,
                                    asio::transfer_all(),
                                    [this](asio::error_code ec, std::size_t) {
                                        if (ec && ec != asio::error::eof) return;
                                        std::string raw(
                                            asio::buffers_begin(buf_.data()),
                                            asio::buffers_end(buf_.data()));
                                        auto r = parsear(raw);
                                        fmt::print("  [async] {} {} — body: {}\n",
                                            r.status, r.status_text,
                                            r.body.substr(0, std::min<size_t>(80, r.body.size())));
                                    });
                            });
                    });
            });
    }

private:
    tcp::socket     socket_;
    tcp::resolver   resolver_;
    std::string     host_, req_;
    asio::streambuf buf_;
};

void demo_get_assincrono() {
    fmt::print("\n── 5.2 HTTP GET assíncrono ──\n");

    asio::io_context io;

    // Verifica conectividade antes
    asio::error_code ec;
    tcp::resolver resolver(io);
    resolver.resolve("httpbin.org", "80", ec);
    if (ec) { fmt::print("  sem conectividade — pulando\n"); return; }

    HttpGetAsync req(io, "httpbin.org", "/uuid");
    req.iniciar();
    io.run();
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. TCP raw vs libcurl — o que você NÃO quer implementar manualmente
// ─────────────────────────────────────────────────────────────────────────────
void demo_comparativo() {
    fmt::print("\n── 5.3 TCP raw vs libcurl ──\n");
    fmt::print(
        "  O que implementamos aqui:\n"
        "    ✓ Conexão TCP\n"
        "    ✓ Request HTTP/1.1 formatado\n"
        "    ✓ Parsing de status + headers + body\n"
        "\n"
        "  O que falta para HTTP de produção (libcurl cobre tudo):\n"
        "    • HTTPS / TLS               → Asio.SSL resolve, mas é complexo\n"
        "    • Redirects automáticos     → 301, 302, 307...\n"
        "    • Chunked Transfer Encoding → body em pedaços\n"
        "    • Keep-Alive / connection pooling\n"
        "    • Compressão gzip/deflate\n"
        "    • Cookies, autenticação Basic/Bearer\n"
        "    • HTTP/2 multiplexing\n"
        "    • Timeouts, retry, backoff\n"
        "\n"
        "  Conclusão:\n"
        "    TCP raw → entender o protocolo (este demo)\n"
        "    libcurl → produção (já em src/http-client/)\n"
    );
}

inline void run() {
    demo_get_sincrono();
    demo_get_assincrono();
    demo_comparativo();
}

} // namespace demo_http_raw
