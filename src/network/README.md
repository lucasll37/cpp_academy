# network — Programação de Sockets com Asio

## O que é este projeto?

Guia completo de **programação de rede** em C++ usando a biblioteca **Asio** (Boost.Asio ou standalone Asio). Cobre TCP síncrono, TCP assíncrono, UDP, timers e HTTP sobre TCP raw.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| `io_context` | Coração do Asio: event loop, gerencia handlers |
| TCP síncrono | `connect` → `read`/`write` → `close` (bloqueia) |
| TCP assíncrono | `async_connect` → callbacks (não bloqueia) |
| UDP datagramas | Sem conexão, `sendto`/`recvfrom` |
| `steady_timer` | Timers assíncronos sem sleep |
| HTTP raw | Construir requisição HTTP manualmente sobre TCP |

---

## Estrutura de arquivos

```
network/
├── main.cpp            ← executa todos os demos em sequência
├── demo_tcp_sync.hpp   ← TCP: connect, read/write bloqueantes
├── demo_tcp_async.hpp  ← TCP: async_read/async_write com callbacks
├── demo_udp.hpp        ← UDP: datagramas sem conexão
├── demo_timer.hpp      ← timers assíncronos
└── demo_http_raw.hpp   ← HTTP/1.1 manual sobre TCP
```

---

## `io_context` — o event loop

```cpp
asio::io_context io;

// io.run() → bloqueia até não haver mais trabalho pendente
// Processa todos os handlers de operações assíncronas completadas
io.run();

// Para operações assíncronas que não terminam naturalmente:
asio::executor_work_guard<asio::io_context::executor_type> work(io.get_executor());
std::thread t([&]{ io.run(); });
// ... enfileira operações ...
work.reset(); // permite que io.run() termine
t.join();
```

---

## Demo 1: TCP Síncrono

```cpp
void demo_tcp_sync::run() {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);

    // Resolver: DNS lookup
    asio::ip::tcp::resolver resolver(io);
    auto endpoints = resolver.resolve("httpbin.org", "80");

    // connect: bloqueia até conectar ou errar
    asio::connect(socket, endpoints);

    // Envia requisição HTTP/1.0
    std::string request =
        "GET /get HTTP/1.0\r\n"
        "Host: httpbin.org\r\n"
        "Connection: close\r\n"
        "\r\n";
    asio::write(socket, asio::buffer(request));

    // Lê resposta completa (até EOF)
    std::string response;
    asio::error_code ec;
    asio::read(socket, asio::dynamic_buffer(response), ec);
    // ec == asio::error::eof é esperado (server fecha a conexão)

    fmt::print("resposta:\n{}\n", response.substr(0, 200));
}
```

---

## Demo 2: TCP Assíncrono

```cpp
// Diferença fundamental: não bloqueia — o callback é chamado quando completo
void demo_tcp_async::run() {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    asio::ip::tcp::resolver resolver(io);

    // Resolver assíncrono: callback chamado quando DNS resolver
    resolver.async_resolve("httpbin.org", "80",
        [&](asio::error_code ec, asio::ip::tcp::resolver::results_type endpoints) {
            if (ec) return;

            // Conecta assincronamente
            asio::async_connect(socket, endpoints,
                [&](asio::error_code ec, asio::ip::tcp::endpoint) {
                    if (ec) return;

                    // Escreve assincronamente
                    auto req = std::make_shared<std::string>("GET / HTTP/1.0\r\n\r\n");
                    asio::async_write(socket, asio::buffer(*req),
                        [&, req](asio::error_code ec, std::size_t) {
                            if (ec) return;
                            // Lê resposta...
                        });
                });
        });

    io.run(); // processa todos os eventos
}
```

### Cadeia de callbacks (callback hell) → Coroutines (C++20)

```cpp
// Com Asio coroutines (mais legível que callbacks aninhados)
asio::co_spawn(io, [&]() -> asio::awaitable<void> {
    auto [ec, endpoints] = co_await resolver.async_resolve("httpbin.org", "80",
                               asio::as_tuple(asio::use_awaitable));
    co_await asio::async_connect(socket, endpoints, asio::use_awaitable);
    co_await asio::async_write(socket, asio::buffer(request), asio::use_awaitable);
    // ...
}, asio::detached);
```

---

## Demo 3: UDP — Datagramas

```cpp
void demo_udp::run() {
    asio::io_context io;

    // Socket UDP (sem conexão)
    asio::ip::udp::socket socket(io, asio::ip::udp::v4());

    // Endpoint de destino
    asio::ip::udp::endpoint dest(
        asio::ip::make_address("8.8.8.8"),
        53  // porta DNS
    );

    // Envia datagrama (fire and forget)
    std::string msg = "hello";
    socket.send_to(asio::buffer(msg), dest);

    // Recebe resposta (com timeout via timer)
    char buf[1024];
    asio::ip::udp::endpoint sender;
    std::size_t n = socket.receive_from(asio::buffer(buf), sender);

    fmt::print("recebido {} bytes de {}\n", n, sender.address().to_string());
}
```

### TCP vs UDP

| Característica | TCP | UDP |
|---|---|---|
| Conexão | Sim (handshake) | Não |
| Confiabilidade | Sim (retransmissão) | Não |
| Ordem | Sim | Não garantida |
| Overhead | Maior | Menor |
| Casos de uso | HTTP, banco de dados | DNS, jogos, vídeo ao vivo |

---

## Demo 4: Timers Assíncronos

```cpp
void demo_timer::run() {
    asio::io_context io;

    // steady_timer: usa std::chrono::steady_clock (monotônico)
    asio::steady_timer timer(io);

    // Timer de 1 segundo
    timer.expires_after(std::chrono::seconds(1));
    timer.async_wait([](asio::error_code ec) {
        if (!ec) fmt::print("1 segundo passou!\n");
    });

    // Timer periódico
    std::function<void(asio::error_code)> tick;
    tick = [&](asio::error_code ec) {
        if (ec) return;
        fmt::print("tick!\n");
        timer.expires_after(std::chrono::milliseconds(500));
        timer.async_wait(tick);
    };
    timer.async_wait(tick);

    io.run();
}
```

---

## Demo 5: HTTP Raw sobre TCP

```cpp
void demo_http_raw::run() {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);

    // Resolve e conecta
    asio::ip::tcp::resolver resolver(io);
    asio::connect(socket, resolver.resolve("httpbin.org", "80"));

    // Constrói requisição HTTP/1.1 manualmente
    std::string request =
        "GET /json HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "User-Agent: cpp_academy/1.0\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "\r\n";

    asio::write(socket, asio::buffer(request));

    // Lê resposta
    asio::streambuf response;
    asio::read_until(socket, response, "\r\n"); // status line

    std::istream resp_stream(&response);
    std::string http_version, status_message;
    unsigned int status_code;
    resp_stream >> http_version >> status_code;
    std::getline(resp_stream, status_message);

    fmt::print("HTTP/{} {} {}\n", http_version, status_code, status_message);
}
```

---

## Gerenciamento de erros com Asio

```cpp
// Forma 1: exceção (padrão)
try {
    asio::connect(socket, endpoints);
} catch (const asio::system_error& e) {
    fmt::print("erro: {}\n", e.what());
}

// Forma 2: error_code (sem exceção)
asio::error_code ec;
asio::connect(socket, endpoints, ec);
if (ec) {
    fmt::print("erro: {}\n", ec.message());
}
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja network
./bin/network
```

**Requer conexão com a internet** para os demos que fazem conexões externas.

---

## Dependências

- `asio` (standalone) ou `Boost.Asio`
- `fmt` — formatação de saída
