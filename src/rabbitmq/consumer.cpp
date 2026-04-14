#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>
#include <fmt/core.h>

int main()
{
    boost::asio::io_context io;

    fmt::print("[CONSUMER] Starting...\n");

    AMQP::LibBoostAsioHandler handler(io);

    AMQP::Address address("amqp://guest:guest@localhost:5673/");
    AMQP::TcpConnection connection(&handler, address);
    AMQP::TcpChannel channel(&connection);

    connection.onConnected([]() {
        fmt::print("[CONSUMER] TCP connected\n");
    });

    connection.onReady([]() {
        fmt::print("[CONSUMER] AMQP ready\n");
    });

    connection.onError([](const char* msg) {
        fmt::print("[CONSUMER][ERROR] {}\n", msg);
    });

    channel.onError([](const char* msg) {
        fmt::print("[CONSUMER][CHANNEL ERROR] {}\n", msg);
    });

    channel.onReady([&]() {
        fmt::print("[CONSUMER] Channel ready\n");

        channel.declareQueue("hello")
            .onSuccess([&](const std::string& name, uint32_t, uint32_t) {
                fmt::print("[CONSUMER] Queue declared: {}\n", name);

                channel.consume("hello")
                    .onReceived([&](const AMQP::Message& msg, uint64_t tag, bool)
                    {
                        std::string body(msg.body(), msg.bodySize());

                        fmt::print("[CONSUMER] Received: {}\n", body);

                        channel.ack(tag);

                        fmt::print("[CONSUMER] ACK sent\n");
                    });
            });
    });

    io.run();

    fmt::print("[CONSUMER] Exiting...\n");
}