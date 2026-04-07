// =============================================================================
//  src/network/demo_udp.hpp
//
//  UDP com Asio standalone
//  ────────────────────────
//  UDP (User Datagram Protocol):
//  • Sem conexão — cada datagrama é independente
//  • Sem garantia de entrega ou ordem
//  • Overhead mínimo — ideal para jogos, DNS, streaming, telemetria
//
//  Diferença chave vs TCP:
//  • TCP: connect() → send()/recv() → close()   (fluxo de bytes)
//  • UDP: send_to(addr) / receive_from(&addr)    (datagramas individuais)
//
//  Tópicos:
//  • send_to / receive_from síncrono
//  • async_receive_from / async_send_to
//  • UDP "conectado" — connect() fixa o endpoint remoto padrão
// =============================================================================
#pragma once

#define ASIO_STANDALONE
#include <asio.hpp>

#include <fmt/core.h>
#include <thread>
#include <chrono>
#include <array>
#include <string>

namespace demo_udp {

using udp = asio::ip::udp;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. Echo UDP síncrono
// ─────────────────────────────────────────────────────────────────────────────
void udp_server(uint16_t porta) {
    asio::io_context io;

    // Socket UDP bound à porta — sem listen(), sem accept()
    udp::socket socket(io, udp::endpoint(udp::v4(), porta));
    fmt::print("  [udp server] aguardando datagramas na porta {}\n", porta);

    for (int i = 0; i < 3; ++i) {
        std::array<char, 256> buf{};
        udp::endpoint remetente; // receive_from preenche o endereço do remetente

        std::size_t n = socket.receive_from(asio::buffer(buf), remetente);
        std::string msg(buf.data(), n);

        fmt::print("  [udp server] '{}' de {}:{}\n",
                   msg,
                   remetente.address().to_string(),
                   remetente.port());

        // Ecoa de volta — precisa especificar destino em cada envio
        socket.send_to(asio::buffer("ECHO:" + msg), remetente);
    }
}

void udp_client(uint16_t porta) {
    std::this_thread::sleep_for(30ms);

    asio::io_context io;
    udp::socket socket(io, udp::endpoint(udp::v4(), 0)); // porta 0 = SO escolhe

    udp::endpoint servidor(asio::ip::make_address("127.0.0.1"), porta);

    for (const char* msg : {"ping", "hello", "world"}) {
        socket.send_to(asio::buffer(std::string(msg)), servidor);
        fmt::print("  [udp client] enviou: '{}'\n", msg);

        std::array<char, 256> buf{};
        udp::endpoint origem;
        std::size_t n = socket.receive_from(asio::buffer(buf), origem);
        fmt::print("  [udp client] resposta: '{}'\n", std::string(buf.data(), n));
    }
}

void demo_echo_udp() {
    fmt::print("\n── 3.1 Echo UDP síncrono ──\n");
    constexpr uint16_t PORTA = 12347;
    std::thread t(udp_server, PORTA);
    udp_client(PORTA);
    t.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. UDP assíncrono
// ─────────────────────────────────────────────────────────────────────────────
void demo_udp_async() {
    fmt::print("\n── 3.2 UDP assíncrono ──\n");

    constexpr uint16_t PORTA = 12348;
    asio::io_context io;

    udp::socket server_sock(io, udp::endpoint(udp::v4(), PORTA));
    std::array<char, 256> buf{};
    udp::endpoint remetente;
    int recebidos = 0;

    std::function<void()> iniciar;
    iniciar = [&]() {
        server_sock.async_receive_from(
            asio::buffer(buf), remetente,
            [&](asio::error_code ec, std::size_t n) {
                if (ec) return;
                std::string msg(buf.data(), n);
                fmt::print("  [async udp] recebido: '{}'\n", msg);
                server_sock.send_to(asio::buffer(std::string("OK")), remetente);
                ++recebidos;
                if (recebidos < 2) iniciar();
            }
        );
    };
    iniciar();

    std::thread client_t([&]() {
        std::this_thread::sleep_for(20ms);
        asio::io_context cio;
        udp::socket sock(cio, udp::endpoint(udp::v4(), 0));
        udp::endpoint dest(asio::ip::make_address("127.0.0.1"), PORTA);

        for (const char* msg : {"async-1", "async-2"}) {
            sock.send_to(asio::buffer(std::string(msg)), dest);
            std::array<char, 64> rbuf{};
            udp::endpoint orig;
            std::size_t n = sock.receive_from(asio::buffer(rbuf), orig);
            fmt::print("  [async udp client] resposta: '{}'\n",
                       std::string(rbuf.data(), n));
        }
    });

    io.run();
    client_t.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. UDP "conectado" — connect() fixa o endpoint remoto
//     Após connect(): usa send()/receive() sem especificar endereço.
//     Kernel filtra automaticamente — só aceita datagramas daquele endpoint.
// ─────────────────────────────────────────────────────────────────────────────
void demo_udp_conectado() {
    fmt::print("\n── 3.3 UDP 'conectado' (endpoint fixo) ──\n");

    constexpr uint16_t PORTA = 12349;

    std::thread srv([&]() {
        asio::io_context io;
        udp::socket sock(io, udp::endpoint(udp::v4(), PORTA));
        std::array<char, 256> buf{};
        udp::endpoint rem;
        std::size_t n = sock.receive_from(asio::buffer(buf), rem);
        fmt::print("  [srv] recebeu: '{}'\n", std::string(buf.data(), n));
        sock.send_to(asio::buffer(std::string("pong")), rem);
    });

    std::this_thread::sleep_for(20ms);

    asio::io_context io;
    udp::socket sock(io, udp::endpoint(udp::v4(), 0));

    // connect() em UDP: apenas registra endpoint remoto padrão (sem handshake)
    sock.connect(udp::endpoint(asio::ip::make_address("127.0.0.1"), PORTA));

    sock.send(asio::buffer(std::string("ping")));
    fmt::print("  [cli] enviou 'ping' via send() sem endereço explícito\n");

    std::array<char, 64> buf{};
    std::size_t n = sock.receive(asio::buffer(buf));
    fmt::print("  [cli] recebeu: '{}'\n", std::string(buf.data(), n));

    srv.join();
}

inline void run() {
    demo_echo_udp();
    demo_udp_async();
    demo_udp_conectado();
}

} // namespace demo_udp
