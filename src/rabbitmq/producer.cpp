#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>
#include <fmt/core.h>

int main()
{
    boost::asio::io_context io;

    fmt::print("[PRODUCER] Starting...\n");

    AMQP::LibBoostAsioHandler handler(io);

    AMQP::Address address("amqp://guest:guest@localhost:5673/");
    AMQP::TcpConnection connection(&handler, address);
    AMQP::TcpChannel channel(&connection);

    connection.onConnected([]() {
        fmt::print("[PRODUCER] TCP connected to broker\n");
    });

    connection.onReady([]() {
        fmt::print("[PRODUCER] AMQP connection ready\n");
    });

    connection.onError([](const char* msg) {
        fmt::print("[PRODUCER][ERROR] {}\n", msg);
    });

    channel.onError([](const char* msg) {
        fmt::print("[PRODUCER][CHANNEL ERROR] {}\n", msg);
    });

    channel.onReady([&]() {
        fmt::print("[PRODUCER] Channel ready\n");

        channel.declareQueue("hello")
            .onSuccess([](const std::string& name, uint32_t, uint32_t) {
                fmt::print("[PRODUCER] Queue declared: {}\n", name);
            });

        std::string msg = "Hello from producer";

        channel.publish("", "hello", msg);

        fmt::print("[PRODUCER] Sent: {}\n", msg);
    });

    io.run();

    fmt::print("[PRODUCER] Exiting...\n");
}