// =============================================================================
//  src/network/demo_tcp_sync.hpp
//
//  TCP Síncrono com Asio (standalone, header-only)
//  ─────────────────────────────────────────────────
//  "Síncrono" = cada operação BLOQUEIA até completar.
//  Simples de entender — uma thread por conexão.
//
//  O que é um socket?
//  Uma "tomada de rede" — conecta dois programas para trocar dados.
//  Servidor: fica escutando em uma porta, esperando conexões.
//  Cliente:  toma a iniciativa de conectar.
//  Porta:    número (0–65535) que identifica o serviço na máquina.
//
//  TCP garante entrega ordenada e sem perda — ideal para HTTP, banco,
//  qualquer coisa onde perder um byte é inaceitável.
//
//  Tópicos:
//  • io_context — o motor do Asio
//  • tcp::acceptor — servidor que aceita conexões
//  • tcp::socket — conexão individual (RAII: fecha no destruidor)
//  • asio::write / asio::read_until
//  • tcp::resolver — traduz hostname → endereço IP
//  • Echo server + client na mesma demo
// =============================================================================
#pragma once

// ASIO_STANDALONE: usa Asio sem Boost (sem boost::system::error_code, etc.)
#define ASIO_STANDALONE
#include <asio.hpp>

#include <fmt/core.h>
#include <thread>
#include <string>
#include <chrono>

namespace demo_tcp_sync {

using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. Echo server síncrono — aceita uma conexão, ecoa tudo, fecha
// ─────────────────────────────────────────────────────────────────────────────
void echo_server(uint16_t porta) {
    // io_context: o "runtime" do Asio — toda operação de I/O precisa de um.
    // Em código síncrono é quase transparente; em async é o event loop.
    asio::io_context io;

    // acceptor: fica "ouvindo" na porta (equivale a socket+bind+listen do BSD)
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), porta));

    fmt::print("  [server] aguardando conexão na porta {}...\n", porta);

    // accept() BLOQUEIA até um cliente conectar (equivale ao accept() do BSD)
    tcp::socket socket(io);
    acceptor.accept(socket);

    fmt::print("  [server] cliente conectado de {}:{}\n",
               socket.remote_endpoint().address().to_string(),
               socket.remote_endpoint().port());

    try {
        while (true) {
            asio::streambuf buf;

            // read_until: lê bytes até encontrar '\n' — bloqueia se necessário
            std::size_t n = asio::read_until(socket, buf, '\n');

            std::string linha(
                asio::buffers_begin(buf.data()),
                asio::buffers_begin(buf.data()) + static_cast<std::ptrdiff_t>(n)
            );
            buf.consume(n);

            fmt::print("  [server] recebido: '{}'", linha);

            if (linha == "FIM\n") {
                asio::write(socket, asio::buffer("BYE\n", 4));
                break;
            }

            // Ecoa de volta
            asio::write(socket, asio::buffer(linha));
        }
    } catch (const asio::system_error& e) {
        if (e.code() != asio::error::eof)
            fmt::print("  [server] erro: {}\n", e.what());
    }

    fmt::print("  [server] conexão encerrada\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Cliente síncrono — conecta, envia mensagens, lê respostas
// ─────────────────────────────────────────────────────────────────────────────
void echo_client(uint16_t porta) {
    std::this_thread::sleep_for(50ms); // garante que server está pronto

    asio::io_context io;
    tcp::socket socket(io);

    // resolver: traduz "127.0.0.1:porta" → endpoint concreto (como getaddrinfo)
    tcp::resolver resolver(io);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(porta));

    // connect(): tenta cada endpoint resolvido até conseguir
    asio::connect(socket, endpoints);
    fmt::print("  [client] conectado ao servidor\n");

    std::vector<std::string> mensagens = {
        "olá servidor\n",
        "como vai?\n",
        "FIM\n"
    };

    for (const auto& msg : mensagens) {
        asio::write(socket, asio::buffer(msg));
        fmt::print("  [client] enviado: '{}'", msg);

        asio::streambuf buf;
        asio::read_until(socket, buf, '\n');
        std::string resp(
            asio::buffers_begin(buf.data()),
            asio::buffers_end(buf.data())
        );
        fmt::print("  [client] resposta: '{}'", resp);
    }

    fmt::print("  [client] encerrando\n");
}

void demo_echo() {
    fmt::print("\n── 1.1 Echo server/client síncrono ──\n");
    constexpr uint16_t PORTA = 12345;
    std::thread server_thread(echo_server, PORTA);
    echo_client(PORTA);
    server_thread.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Resolução de nomes
// ─────────────────────────────────────────────────────────────────────────────
void demo_resolucao() {
    fmt::print("\n── 1.2 Resolução de nomes ──\n");

    asio::io_context io;
    tcp::resolver resolver(io);

    asio::error_code ec;
    auto results = resolver.resolve("localhost", "80", ec);

    if (!ec) {
        fmt::print("  'localhost:80' resolve para:\n");
        for (const auto& r : results) {
            fmt::print("    {} ({})\n",
                       r.endpoint().address().to_string(),
                       r.endpoint().protocol() == tcp::v4() ? "IPv4" : "IPv6");
        }
    } else {
        fmt::print("  erro ao resolver: {}\n", ec.message());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Error handling: exceção vs error_code
//     Asio suporta dois estilos:
//     a) Lança exceção (padrão) — mais simples
//     b) Preenche error_code   — sem overhead de exceção, bom em hot paths
// ─────────────────────────────────────────────────────────────────────────────
void demo_error_code() {
    fmt::print("\n── 1.3 Error handling: error_code (sem exceção) ──\n");

    asio::io_context io;
    tcp::socket socket(io);
    tcp::resolver resolver(io);

    asio::error_code ec;
    auto endpoints = resolver.resolve("127.0.0.1", "9999", ec);
    if (ec) { fmt::print("  resolve erro: {}\n", ec.message()); return; }

    asio::connect(socket, endpoints, ec);
    if (ec) {
        fmt::print("  connect falhou (esperado — porta 9999 fechada): {}\n",
                   ec.message());
        fmt::print("  categoria: '{}', valor: {}\n",
                   ec.category().name(), ec.value());
    }
}

inline void run() {
    demo_echo();
    demo_resolucao();
    demo_error_code();
}

} // namespace demo_tcp_sync
