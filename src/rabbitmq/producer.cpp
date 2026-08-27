// =============================================================================
//  src/rabbitmq/producer.cpp
//
//  O produtor — publica mensagens numa fila do RabbitMQ
//  ────────────────────────────────────────────────────
//  ⚠ Precisa de um RabbitMQ rodando; ver o cabeçalho de consumer.cpp.
//
//        docker run -d --rm --name rabbit 
//            -p 5672:5672 -p 15672:15672 rabbitmq:3-management
// =============================================================================
#include "handler.hpp"

#include <amqpcpp.h>
#include <boost/asio.hpp>
#include <fmt/core.h>

#include <chrono>
#include <string>

int main(int argc, char** argv) {
    const std::string url =
        argc > 1 ? argv[1] : "amqp://guest:guest@localhost:5672/";
    const int quantas = argc > 2 ? std::atoi(argv[2]) : 5;

    fmt::print("[PRODUCER] conectando em {}\n", url);

    boost::asio::io_context io;
    rabbit::Handler         handler{io, "PRODUCER"};

    AMQP::Address       address{url};
    AMQP::TcpConnection connection{&handler, address};
    AMQP::TcpChannel    channel{&connection};

    channel.onError([](const char* mensagem) {
        fmt::print("[PRODUCER] ⚠ erro de canal: {}\n", mensagem);
    });

    channel.onReady([&channel, &connection, quantas] {
        fmt::print("[PRODUCER] canal pronto\n");

        channel.declareQueue("hello")
            .onSuccess([&channel, &connection, quantas](
                           const std::string& nome, uint32_t, uint32_t) {
                fmt::print("[PRODUCER] fila '{}' pronta\n", nome);

                for (int i = 1; i <= quantas; ++i) {
                    const std::string corpo =
                        fmt::format("mensagem {} de {}", i, quantas);

                    // ⚠ Os argumentos são (EXCHANGE, ROUTING KEY, corpo).
                    //   A exchange VAZIA ("") é a *default exchange*, um caso
                    //   especial do AMQP: ela roteia a mensagem para a fila
                    //   cujo NOME é igual à routing key. É por isso que o
                    //   segundo argumento aqui é "hello" — o nome da fila.
                    //   Numa aplicação real você declara a sua exchange e usa
                    //   routing keys de verdade.
                    channel.publish("", nome, corpo);
                    fmt::print("[PRODUCER] publicado: \"{}\"\n", corpo);
                }

                // ⚠ `publish` NÃO confirma nada por padrão: ele só escreve no
                //   socket. Se o servidor rejeitar, você não fica sabendo.
                //   Para garantia de entrega existe o *publisher confirm*
                //   (`channel.confirmSelect()`), que devolve um deferred.
                //   É o mesmo trade-off de `send` vs `send + ack` em qualquer
                //   protocolo — ver src/network/.
                fmt::print("[PRODUCER] {} mensagem(ns) enviada(s); fechando\n",
                           quantas);
                connection.close();
            })
            .onError([](const char* mensagem) {
                fmt::print("[PRODUCER] ⚠ erro ao declarar a fila: {}\n", mensagem);
            });
    });

    const int segundos = argc > 3 ? std::atoi(argv[3]) : 15;
    boost::asio::steady_timer limite{io, std::chrono::seconds{segundos}};
    limite.async_wait([&io](const boost::system::error_code&) { io.stop(); });

    io.run();

    fmt::print("[PRODUCER] fim\n");
    return handler.erro() ? 1 : 0;
}
