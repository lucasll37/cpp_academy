// =============================================================================
//  src/rabbitmq/handler.hpp
//
//  O handler do AMQP-CPP — e por que ele é uma CLASSE, não lambdas
//  ───────────────────────────────────────────────────────────────
//  A primeira versão deste subprojeto escrevia assim:
//
//      AMQP::TcpConnection connection(&handler, address);
//      connection.onConnected([]{ ... });      // ❌ não existe
//      connection.onError([](const char* m){ ... });
//
//  e não compilava:
//
//      error: cannot convert 'main()::<lambda()>' to 'AMQP::TcpState*'
//
//  A mensagem é confusa porque `TcpConnection` TEM métodos com esses nomes —
//  mas eles são outra coisa (parte da máquina de estados interna). Os eventos
//  do CICLO DE VIDA da conexão pertencem ao HANDLER, e a forma de reagir a
//  eles é HERDAR e sobrescrever métodos virtuais.
//
//  É a diferença entre dois estilos de callback, e vale entender qual usar:
//
//    LAMBDA por evento      → um callback INDEPENDENTE por chamada.
//                             Usado nas operações do canal (`declareQueue`,
//                             `consume`), onde cada chamada tem o seu retorno.
//    HERANÇA com virtual    → um conjunto FIXO e conhecido de eventos, todos
//                             pertencentes ao mesmo objeto, com estado
//                             compartilhado entre eles.
//
//  Ver src/oop/demo_virtual.hpp (o mecanismo) e src/type_erasure/ (quando
//  `std::function` substitui a herança — e quando não substitui).
// =============================================================================
#pragma once

#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>

#include <fmt/core.h>

#include <string>

namespace rabbit {

// Herdamos do handler do Boost.Asio, que já implementa a integração com o
// `io_context` (o loop de eventos). Só sobrescrevemos os eventos que nos
// interessam — todos os métodos da base têm implementação vazia por padrão,
// o que é o idioma correto para uma interface de OBSERVAÇÃO.
class Handler : public AMQP::LibBoostAsioHandler {
public:
    Handler(boost::asio::io_context& io, std::string rotulo)
        : AMQP::LibBoostAsioHandler{io}, rotulo_{std::move(rotulo)} {}

    // ⚠ O ciclo de vida tem SEIS estágios, e confundi-los é a origem da
    //   maioria dos bugs de "publiquei e a mensagem sumiu":
    //
    //     onAttached   objeto construído — ainda não há socket
    //     onConnected  TCP estabelecido — ainda NÃO há login AMQP
    //     onReady      handshake AMQP concluído  ← só AQUI dá para publicar
    //     onError      erro fatal; a conexão vai fechar
    //     onClosed     fechamento limpo
    //     onDetached   pode destruir o objeto
    //
    //   Publicar em `onConnected` é cedo demais: o canal ainda não existe.

    void onAttached(AMQP::TcpConnection* c) override {
        AMQP::LibBoostAsioHandler::onAttached(c);   // a base faz trabalho real
        fmt::print("[{}] onAttached — objeto de conexão criado\n", rotulo_);
    }

    void onConnected(AMQP::TcpConnection* c) override {
        AMQP::LibBoostAsioHandler::onConnected(c);
        fmt::print("[{}] onConnected — TCP estabelecido (AMQP ainda NÃO)\n",
                   rotulo_);
    }

    void onReady(AMQP::TcpConnection* c) override {
        AMQP::LibBoostAsioHandler::onReady(c);
        pronto_ = true;
        fmt::print("[{}] onReady — handshake AMQP concluído\n", rotulo_);
    }

    void onError(AMQP::TcpConnection* c, const char* mensagem) override {
        AMQP::LibBoostAsioHandler::onError(c, mensagem);
        erro_ = true;
        fmt::print("[{}] ⚠ onError — {}\n", rotulo_, mensagem);
    }

    void onClosed(AMQP::TcpConnection* c) override {
        AMQP::LibBoostAsioHandler::onClosed(c);
        fmt::print("[{}] onClosed — conexão encerrada\n", rotulo_);
    }

    void onDetached(AMQP::TcpConnection* c) override {
        AMQP::LibBoostAsioHandler::onDetached(c);
        fmt::print("[{}] onDetached — seguro destruir\n", rotulo_);
    }

    bool pronto() const noexcept { return pronto_; }
    bool erro()   const noexcept { return erro_; }

private:
    std::string rotulo_;
    bool        pronto_ = false;
    bool        erro_   = false;
};

// ⚠ Chamar a implementação da base em cada override NÃO é opcional aqui: a
//   `LibBoostAsioHandler` faz trabalho real nesses métodos (registra o socket
//   no `io_context`, monitora leitura/escrita). Esquecer o
//   `AMQP::LibBoostAsioHandler::onX(...)` produz um programa que compila,
//   conecta, e nunca recebe nada.
//
//   É a armadilha clássica de sobrescrever um virtual que NÃO é puro: o
//   compilador não avisa, porque tecnicamente você o implementou.
//   Ver src/oop/demo_virtual.hpp e src/solid/lsp/ (NVI evita exatamente isso).

}  // namespace rabbit
