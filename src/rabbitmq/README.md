# rabbitmq — Mensageria Assíncrona com RabbitMQ

## O que é este projeto?

Demonstra comunicação assíncrona via **RabbitMQ** usando a biblioteca **AMQP-CPP** com backend Boost.Asio. Implementa o padrão **produtor/consumidor** clássico: um processo envia mensagens para uma fila, outro as consome.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| Message broker | RabbitMQ como intermediário entre produtor e consumidor |
| AMQP protocol | Protocolo de mensageria padrão |
| Exchange + Queue | Como mensagens são roteadas no broker |
| Conexão assíncrona | Boost.Asio como backend de I/O |
| `declareQueue` | Criação idempotente de fila |
| `publish` | Envio de mensagem |
| `consume` | Assinatura de fila para recebimento |

---

## Estrutura de arquivos

```
rabbitmq/
├── producer.cpp    ← conecta ao broker e publica mensagem
├── consumer.cpp    ← conecta ao broker e consome mensagens
└── meson.build
```

---

## O Produtor — `producer.cpp`

```cpp
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>

int main() {
    boost::asio::io_context io;

    // LibBoostAsioHandler: adapta Boost.Asio como backend do AMQP-CPP
    AMQP::LibBoostAsioHandler handler(io);

    // Endereço do broker: amqp://user:password@host:port/vhost
    AMQP::Address address("amqp://guest:guest@localhost:5673/");
    AMQP::TcpConnection connection(&handler, address);
    AMQP::TcpChannel channel(&connection);

    // Callbacks de conexão
    connection.onConnected([]()  { fmt::print("[PRODUCER] TCP connected\n"); });
    connection.onReady([]()      { fmt::print("[PRODUCER] AMQP ready\n"); });
    connection.onError([](const char* msg) {
        fmt::print("[PRODUCER][ERROR] {}\n", msg);
    });

    // Quando o canal estiver pronto:
    channel.onReady([&]() {
        // Declara a fila (idempotente — cria se não existe)
        channel.declareQueue("hello")
            .onSuccess([](const std::string& name, uint32_t, uint32_t) {
                fmt::print("[PRODUCER] Queue '{}' declarada\n", name);
            });

        // Publica mensagem na exchange padrão ("") para a fila "hello"
        channel.publish("", "hello", "Hello from C++ producer!");
        fmt::print("[PRODUCER] Mensagem publicada\n");
    });

    io.run(); // event loop: processa conexão + publicação
    return 0;
}
```

---

## O Consumidor — `consumer.cpp`

```cpp
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>

int main() {
    boost::asio::io_context io;
    AMQP::LibBoostAsioHandler handler(io);
    AMQP::TcpConnection connection(&handler,
        AMQP::Address("amqp://guest:guest@localhost:5673/"));
    AMQP::TcpChannel channel(&connection);

    channel.onReady([&]() {
        // Declara a mesma fila (idempotente)
        channel.declareQueue("hello");

        // Assina a fila para receber mensagens
        channel.consume("hello")
            .onReceived([&](const AMQP::Message& msg, uint64_t tag, bool redelivered) {
                // Mensagem recebida!
                std::string body(msg.body(), msg.bodySize());
                fmt::print("[CONSUMER] Recebido: {}\n", body);

                // ACK: confirma processamento ao broker
                // Sem ACK, broker reenvia a mensagem (pode ser intencional)
                channel.ack(tag);
            })
            .onError([](const char* msg) {
                fmt::print("[CONSUMER] Erro no consume: {}\n", msg);
            });

        fmt::print("[CONSUMER] Aguardando mensagens...\n");
    });

    io.run(); // event loop: aguarda mensagens indefinidamente
    return 0;
}
```

---

## O modelo AMQP — Exchange, Queue, Binding

```
Produtor → Exchange → Binding → Queue → Consumidor

Exchange tipos:
  direct   → rota para a queue com routing_key igual à mensagem
  fanout   → rota para TODAS as queues vinculadas
  topic    → rota por padrão (routing_key com wildcards * e #)
  headers  → rota por cabeçalhos da mensagem

No exemplo:
  channel.publish("", "hello", msg)
         │         │    │
  exchange default  │    │
  ("" = direct)   queue  mensagem
                  name = routing_key
```

---

## Padrões de mensageria

### Work Queue (usado aqui)

```
Produtor → Queue → Consumidor 1
                  → Consumidor 2  (round-robin entre consumidores)
```

### Publish/Subscribe

```
Produtor → Exchange (fanout) → Queue A → Consumidor A
                              → Queue B → Consumidor B
```

### Request/Reply (RPC)

```
Solicitante → Queue de requests → Respondedor
Solicitante ← Queue de replies  ←
```

---

## ACK — confirmação de mensagem

```
ACK:
  channel.ack(delivery_tag);
  → Broker remove a mensagem da queue (processada com sucesso)

NACK (reject):
  channel.nack(delivery_tag);
  → Broker pode reencaminhar para outro consumidor ou mover para dead-letter queue

NACK com requeue:
  channel.nack(delivery_tag, false, true); // true = requeue
  → Mensagem volta para a frente da queue
```

---

## Configuração do RabbitMQ

```bash
# Via Docker (recomendado)
docker run -d \
    --name rabbitmq \
    -p 5672:5672 \
    -p 5673:5673 \
    -p 15672:15672 \
    rabbitmq:management

# Management UI disponível em http://localhost:15672
# Credenciais padrão: guest/guest
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja rabbitmq_producer rabbitmq_consumer

# Terminal 1: consumidor
./bin/rabbitmq_consumer

# Terminal 2: produtor
./bin/rabbitmq_producer
```

**Requer**: RabbitMQ rodando em `localhost:5673` (ou ajustar o endereço no código).

---

## Dependências

- `amqpcpp` — biblioteca AMQP para C++
- `Boost.Asio` — backend de I/O assíncrono
- `fmt` — formatação de saída
- RabbitMQ broker (externo)
