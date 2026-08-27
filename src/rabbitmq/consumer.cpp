// =============================================================================
//  src/rabbitmq/consumer.cpp
//
//  O consumidor — recebe mensagens de uma fila do RabbitMQ
//  ───────────────────────────────────────────────────────
//  ⚠ Este programa precisa de um RabbitMQ RODANDO. Ele não é um teste
//    unitário: é um cliente de rede. Sem servidor, ele imprime o erro de
//    conexão e sai — o que já é informação útil.
//
//        docker run -d --rm --name rabbit 
//            -p 5672:5672 -p 15672:15672 rabbitmq:3-management
//        ./dist/bin/rabbitMQ_consumer          # num terminal
//        ./dist/bin/rabbitMQ_producer          # noutro
//
//  A porta padrão do AMQP é 5672. O painel web fica em 15672 (guest/guest).
//
//  O modelo mental, em uma frase: o PRODUTOR publica numa EXCHANGE, que
//  roteia para uma FILA, de onde o CONSUMIDOR lê. Produtor e consumidor nunca
//  se falam diretamente, e é isso que os desacopla — nenhum precisa estar no
//  ar quando o outro publica.
// =============================================================================
#include "handler.hpp"

#include <amqpcpp.h>
#include <boost/asio.hpp>
#include <fmt/core.h>

#include <chrono>
#include <string>

int main(int argc, char** argv) {
    // Permite apontar para outro servidor sem recompilar.
    const std::string url =
        argc > 1 ? argv[1] : "amqp://guest:guest@localhost:5672/";

    fmt::print("[CONSUMER] conectando em {}\n", url);

    boost::asio::io_context io;
    rabbit::Handler         handler{io, "CONSUMER"};

    AMQP::Address       address{url};
    AMQP::TcpConnection connection{&handler, address};
    AMQP::TcpChannel    channel{&connection};

    // ── Callbacks do CANAL ───────────────────────────────────────────────────
    //  Aqui são lambdas, e não virtuais: cada operação de canal tem o seu
    //  próprio retorno assíncrono. Ver a discussão em handler.hpp.
    channel.onError([](const char* mensagem) {
        fmt::print("[CONSUMER] ⚠ erro de canal: {}\n", mensagem);
    });

    channel.onReady([&channel] {
        fmt::print("[CONSUMER] canal pronto\n");

        // `declareQueue` é IDEMPOTENTE: se a fila já existe com os mesmos
        // parâmetros, não faz nada. Declarar dos DOIS lados é a prática
        // correta — nenhum dos dois precisa saber quem subiu primeiro.
        channel.declareQueue("hello")
            .onSuccess([&channel](const std::string& nome,
                                  uint32_t mensagens, uint32_t consumidores) {
                fmt::print("[CONSUMER] fila '{}' pronta "
                           "({} mensagem(ns), {} consumidor(es))\n",
                           nome, mensagens, consumidores);

                channel.consume(nome)
                    .onReceived([&channel](const AMQP::Message& msg,
                                           uint64_t etiqueta, bool redespachada) {
                        const std::string corpo{msg.body(), msg.bodySize()};

                        fmt::print("[CONSUMER] recebido: \"{}\"{}\n", corpo,
                                   redespachada ? "  (REDESPACHADA)" : "");

                        // ⚠ O ACK é o que remove a mensagem da fila. Sem ele,
                        //   o RabbitMQ a redespacha quando a conexão cai — o
                        //   que é RECURSO, não bug: garante que nada se perde
                        //   se o consumidor morrer no meio do processamento.
                        //   Por isso o ack vem DEPOIS de processar, nunca antes.
                        channel.ack(etiqueta);
                    })
                    .onError([](const char* mensagem) {
                        fmt::print("[CONSUMER] ⚠ erro no consume: {}\n", mensagem);
                    });
            })
            .onError([](const char* mensagem) {
                fmt::print("[CONSUMER] ⚠ erro ao declarar a fila: {}\n", mensagem);
            });
    });

    // ── O loop de eventos ────────────────────────────────────────────────────
    //  `io.run()` só retorna quando não há mais trabalho pendente. Um timer
    //  dá ao programa um limite de vida, para que ele não fique preso para
    //  sempre quando não há servidor — e para que sirva de teste de fumaça.
    const int segundos = argc > 2 ? std::atoi(argv[2]) : 30;
    boost::asio::steady_timer limite{io, std::chrono::seconds{segundos}};
    limite.async_wait([&io, segundos](const boost::system::error_code&) {
        fmt::print("[CONSUMER] limite de {} s atingido, encerrando\n", segundos);
        io.stop();
    });

    io.run();

    fmt::print("[CONSUMER] fim\n");
    return handler.erro() ? 1 : 0;
}
