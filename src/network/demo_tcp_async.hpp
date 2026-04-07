// =============================================================================
//  src/network/demo_tcp_async.hpp
//
//  TCP Assíncrono com Asio standalone
//  ────────────────────────────────────
//  "Assíncrono" = operação INICIA e retorna imediatamente.
//  Um callback (handler) é chamado quando a operação completa.
//  Uma única thread pode gerenciar centenas de conexões.
//
//  Modelo mental:
//    async_read(socket, buf, handler)  ← registra a operação, retorna já
//    io_context.run()                  ← event loop: chama handlers prontos
//
//  Por que isso importa?
//    Síncrono: 1000 conexões = 1000 threads = problema
//    Assíncrono: 1000 conexões = 1 thread + 1000 handlers registrados = ok
//
//  Padrão fundamental — callback chain:
//    async_accept → handler → async_read → handler → async_write → handler
//    → async_read → ... (ciclo enquanto conexão viva)
//
//  Tópicos:
//  • async_accept, async_read_until, async_write
//  • shared_ptr + enable_shared_from_this para lifetime de sessões
//  • Servidor multi-conexão com uma thread
//  • asio::post — postar trabalho no io_context
//  • strand — serializar handlers sem mutex
// =============================================================================
#pragma once

#define ASIO_STANDALONE
#include <asio.hpp>

#include <fmt/core.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

namespace demo_tcp_async {

using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

// =============================================================================
//  Sessão — representa UMA conexão ativa
//
//  Por que shared_ptr?
//  Operações async ficam "pendentes" por tempo indeterminado.
//  Se a Sessao fosse destruída antes da operação completar → crash.
//  shared_ptr garante que o objeto vive enquanto há I/O pendente.
//  shared_from_this() cria um shared_ptr para *this dentro do objeto.
// =============================================================================
class Sessao : public std::enable_shared_from_this<Sessao> {
public:
    explicit Sessao(tcp::socket socket)
        : socket_(std::move(socket)), id_(proximo_id_++) {}

    void iniciar() {
        fmt::print("  [sessão {}] cliente conectado de {}:{}\n",
                   id_,
                   socket_.remote_endpoint().address().to_string(),
                   socket_.remote_endpoint().port());
        ler(); // dispara a chain assíncrona
    }

private:
    // ── Passo 1: registra leitura assíncrona ─────────────────────────────────
    void ler() {
        asio::async_read_until(
            socket_,
            buf_entrada_,
            '\n',
            // Lambda captura shared_ptr para si mesmo — mantém objeto vivo
            [self = shared_from_this()](asio::error_code ec, std::size_t bytes) {
                self->ao_ler(ec, bytes);
            }
        );
        // async_read_until retorna IMEDIATAMENTE aqui
    }

    // ── Passo 2: handler chamado quando '\n' chegou ───────────────────────────
    void ao_ler(asio::error_code ec, std::size_t bytes) {
        if (ec) {
            if (ec != asio::error::eof)
                fmt::print("  [sessão {}] erro leitura: {}\n", id_, ec.message());
            fmt::print("  [sessão {}] desconectado\n", id_);
            return; // shared_ptr vai a zero → Sessao destruída automaticamente
        }

        std::string linha(
            asio::buffers_begin(buf_entrada_.data()),
            asio::buffers_begin(buf_entrada_.data()) + static_cast<std::ptrdiff_t>(bytes)
        );
        buf_entrada_.consume(bytes);
        fmt::print("  [sessão {}] recebido: '{}'", id_, linha);

        saida_ = (linha == "FIM\n") ? "BYE\n" : "ECHO: " + linha;
        escrever();
    }

    // ── Passo 3: registra escrita assíncrona ─────────────────────────────────
    void escrever() {
        asio::async_write(
            socket_,
            asio::buffer(saida_),
            [self = shared_from_this()](asio::error_code ec, std::size_t) {
                self->ao_escrever(ec);
            }
        );
    }

    // ── Passo 4: handler chamado quando escrita completou ────────────────────
    void ao_escrever(asio::error_code ec) {
        if (ec) {
            fmt::print("  [sessão {}] erro escrita: {}\n", id_, ec.message());
            return;
        }
        if (saida_ != "BYE\n")
            ler(); // volta ao passo 1 — ciclo assíncrono
        // se BYE: não chama ler() → shared_ptr cai para zero → destruição
    }

    tcp::socket       socket_;
    asio::streambuf   buf_entrada_;
    std::string       saida_;
    int               id_;
    static inline int proximo_id_ = 1;
};

// =============================================================================
//  Servidor assíncrono — aceita múltiplas conexões com uma só thread
// =============================================================================
class ServidorAsync {
public:
    ServidorAsync(asio::io_context& io, uint16_t porta)
        : acceptor_(io, tcp::endpoint(tcp::v4(), porta))
    {
        fmt::print("  [server async] ouvindo na porta {}\n", porta);
        aceitar(); // registra o primeiro accept
    }

private:
    void aceitar() {
        // async_accept: registra — retorna imediatamente
        // Quando um cliente conectar, o lambda é chamado
        acceptor_.async_accept(
            [this](asio::error_code ec, tcp::socket socket) {
                if (!ec) {
                    // Cria Sessao, shared_ptr gerencia o lifetime
                    std::make_shared<Sessao>(std::move(socket))->iniciar();
                }
                aceitar(); // re-registra para aceitar a próxima conexão
            }
        );
    }

    tcp::acceptor acceptor_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Demo: servidor async em thread separada, 3 clientes em paralelo
// ─────────────────────────────────────────────────────────────────────────────
void demo_servidor_async() {
    fmt::print("\n── 2.1 Servidor assíncrono (múltiplas conexões, 1 thread) ──\n");

    constexpr uint16_t PORTA = 12346;
    asio::io_context io;
    ServidorAsync servidor(io, PORTA);

    // io_context.run() em thread separada — processa todos os eventos async
    std::thread io_thread([&io]() { io.run(); });

    std::this_thread::sleep_for(30ms);

    // 3 clientes síncronos conectando "simultaneamente"
    auto cliente = [&](int id) {
        asio::io_context cio;
        tcp::socket socket(cio);
        tcp::resolver resolver(cio);
        asio::connect(socket, resolver.resolve("127.0.0.1", std::to_string(PORTA)));

        auto troca = [&](const std::string& msg) {
            asio::write(socket, asio::buffer(msg));
            asio::streambuf buf;
            asio::read_until(socket, buf, '\n');
            std::string resp(asio::buffers_begin(buf.data()),
                             asio::buffers_end(buf.data()));
            fmt::print("  [cli {}] '{}' → '{}'\n", id,
                       msg.substr(0, msg.size()-1),
                       resp.substr(0, resp.size()-1));
        };

        troca(fmt::format("hello do cliente {}\n", id));
        troca("FIM\n");
    };

    std::thread c1(cliente, 1);
    std::thread c2(cliente, 2);
    std::thread c3(cliente, 3);
    c1.join(); c2.join(); c3.join();

    io.stop();
    io_thread.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo: asio::post — postar trabalho no io_context de outra thread
//  Útil para "passar mensagem" entre threads sem mutex
// ─────────────────────────────────────────────────────────────────────────────
void demo_post() {
    fmt::print("\n── 2.2 asio::post — trabalho no io_context ──\n");

    asio::io_context io;
    auto guard = asio::make_work_guard(io);

    std::thread io_thread([&io]() { io.run(); });

    // Posta trabalho de qualquer thread — thread-safe
    for (int i = 0; i < 4; ++i) {
        asio::post(io, [i]() {
            fmt::print("  tarefa {} executada no io_context\n", i);
        });
    }

    std::this_thread::sleep_for(30ms);
    guard.reset(); // libera → io.run() pode retornar
    io_thread.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo: strand — serializa handlers sem mutex
//  Se io_context.run() roda em múltiplas threads, handlers da mesma
//  conexão podem executar em paralelo → race condition.
//  strand garante que seus handlers executam em série.
// ─────────────────────────────────────────────────────────────────────────────
void demo_strand() {
    fmt::print("\n── 2.3 strand — serialização de handlers ──\n");

    asio::io_context io;
    asio::io_context::strand strand(io);
    int contador = 0; // compartilhado — seguro porque strand serializa

    for (int i = 0; i < 5; ++i) {
        asio::post(strand, [i, &contador]() {
            ++contador;
            fmt::print("  [strand {}] contador = {}\n", i, contador);
        });
    }

    // 2 threads rodando o mesmo io_context — sem strand haveria race condition
    std::thread t1([&io]() { io.run(); });
    std::thread t2([&io]() { io.run(); });
    t1.join(); t2.join();

    fmt::print("  resultado final = {} (esperado 5, sem race condition)\n", contador);
}

inline void run() {
    demo_servidor_async();
    demo_post();
    demo_strand();
}

} // namespace demo_tcp_async
