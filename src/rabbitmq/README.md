# rabbitmq — mensageria com RabbitMQ e AMQP-CPP, do zero

## O que é este projeto?

Dois programas — um **produtor** e um **consumidor** — que trocam mensagens através de um
broker RabbitMQ usando a biblioteca **AMQP-CPP** sobre o `io_context` do **Boost.Asio**. Eles
nunca se falam diretamente, e é exatamente isso que o subprojeto ensina.

```bash
# 1. subir o broker (Docker)
docker compose -f src/rabbitmq/docker-compose.yml up -d

# 2. compilar
make build

# 3. terminal A — o consumidor fica ouvindo por 30 s
./dist/bin/rabbitMQ_consumer amqp://guest:guest@localhost:4672/

# 4. terminal B — o produtor publica 5 mensagens e sai
./dist/bin/rabbitMQ_producer amqp://guest:guest@localhost:4672/
```

> ⚠ **Os alvos se chamam `rabbitMQ_producer` e `rabbitMQ_consumer`, com M e Q maiúsculos.**
> `rabbitmq_producer` (tudo minúsculo) **não existe**. Confira:
>
> ```bash
> grep -n "executable(" src/rabbitmq/meson.build   # → 'rabbitMQ_producer', 'rabbitMQ_consumer'
> ls dist/bin | grep -i rabbit                     # → rabbitMQ_consumer  rabbitMQ_producer
> ```

> ⚠ **A porta é 4672 no seu host, não 5672.** A porta padrão do AMQP é 5672, e é essa que o
> RabbitMQ escuta *dentro* do contêiner. Mas o
> [`docker-compose.yml`](docker-compose.yml) deste diretório publica `"4672:5672"` — host
> 4672 → contêiner 5672 — justamente para não colidir com um RabbitMQ que você já tenha na
> máquina. Como os dois programas têm `amqp://guest:guest@localhost:5672/` como padrão
> embutido, **com este compose você precisa passar a URL na linha de comando.** É para isso
> que o `argv[1]` existe.

Argumentos aceitos (nenhum é obrigatório):

| Programa | `argv[1]` | `argv[2]` | `argv[3]` |
|---|---|---|---|
| `rabbitMQ_consumer` | URL do broker (padrão `amqp://guest:guest@localhost:5672/`) | segundos até desistir (padrão **30**) | — |
| `rabbitMQ_producer` | URL do broker (mesmo padrão) | quantas mensagens publicar (padrão **5**) | segundos até desistir (padrão **15**) |

Painel web do broker: **http://localhost:14672** (usuário `guest`, senha `guest`).

---

## Antes de começar: os conceitos

Esta seção define **todos** os termos usados adiante. Se "broker", "canal" ou "loop de
eventos" forem novidade, leia-a inteira antes de rodar qualquer coisa.

### 1. Que problema a mensageria resolve

Imagine dois serviços: o **pedido** e o **e-mail de confirmação**. Do jeito direto, o pedido
chama o e-mail por HTTP e espera a resposta:

```
   [pedido] ──HTTP POST /enviar──▶ [email]
            ◀──── 200 OK ─────────
```

Três coisas dão errado nesse desenho, e todas acontecem em produção:

1. **o e-mail cai** → o `POST` falha → o pedido falha. Um serviço secundário derruba um
   serviço essencial;
2. **o e-mail fica lento** → o pedido fica lento junto, porque está bloqueado esperando;
3. **chegam 10.000 pedidos num minuto** → o e-mail recebe 10.000 chamadas simultâneas e
   morre, porque não há onde a demanda esperar.

Com um broker no meio, o pedido escreve numa **fila** e segue a vida:

```
   [pedido] ──publica──▶ ┌───────────┐ ──entrega──▶ [email]
                         │   FILA    │
                         │ ▣ ▣ ▣ ▣ ▣ │  ← as mensagens ESPERAM aqui
                         └───────────┘
```

- o e-mail caiu? as mensagens **esperam** na fila e são entregues quando ele voltar;
- o e-mail é lento? ele consome no ritmo dele; a fila **absorve o pico**;
- precisa de mais vazão? suba um segundo consumidor e o broker divide o trabalho.

É isso que se chama **desacoplamento**: produtor e consumidor não precisam estar no ar ao
mesmo tempo, não precisam conhecer o endereço um do outro e não precisam ter a mesma
velocidade. E não é conversa: [a seção de validação](#como-validar) mostra a fila com **3
mensagens e 0 consumidores** logo depois de o produtor terminar e sair.

| Termo | Definição |
|---|---|
| **broker** | o servidor que fica no meio: recebe, guarda e entrega as mensagens. Aqui, o RabbitMQ. |
| **produtor** | quem publica. Não sabe quem vai ler, nem se alguém vai. |
| **consumidor** | quem lê. Não sabe quem publicou. |
| **mensagem** | um punhado de bytes + propriedades (content-type, prioridade, headers...). O broker não interpreta o corpo. |

### 2. AMQP, conexão e canal

**AMQP 0-9-1** é o protocolo que RabbitMQ e AMQP-CPP falam: um formato binário sobre TCP,
com comandos como `queue.declare`, `basic.publish` e `basic.ack`. Ele tem duas camadas que se
confundem o tempo todo:

```
  ┌─── UMA conexão TCP (cara: 3-way handshake, TLS, autenticação) ───┐
  │                                                                  │
  │   ┌── canal 1 ──┐  ┌── canal 2 ──┐  ┌── canal 3 ──┐              │
  │   │ declareQueue│  │  consume    │  │  publish    │              │
  │   └─────────────┘  └─────────────┘  └─────────────┘              │
  └──────────────────────────────────────────────────────────────────┘
```

- **conexão (`AMQP::TcpConnection`)**: uma conexão TCP com o broker, autenticada. Abrir uma
  custa caro.
- **canal (`AMQP::TcpChannel`)**: uma sessão lógica **dentro** da conexão. Todo comando AMQP
  viaja num canal. Criar um é barato: é só um número no cabeçalho de cada quadro.

A regra prática: **uma conexão por processo, um canal por thread ou por tarefa.** Abrir uma
conexão TCP por thread desperdiça descritores e handshakes; e canais **não** são seguros para
uso concorrente, então dois threads não devem compartilhar o mesmo.

> ⚠ Canal é frágil de propósito: qualquer erro de protocolo (declarar uma fila com parâmetros
> diferentes dos existentes, dar `ack` numa etiqueta inválida) **fecha o canal**, não a
> conexão. Por isso os dois programas registram `channel.onError(...)` — é lá que essa classe
> de erro aparece.

### 3. Exchange, fila, binding e routing key

Um produtor **nunca** publica numa fila. Ele publica numa **exchange**, que é um roteador:

```
                 routing key = "hello"
   [produtor] ──────────────▶ ( exchange ) ──binding──▶ [ fila "hello" ] ──▶ [consumidor]
                                   │
                                   └──binding──────────▶ [ fila "auditoria" ]
```

| Termo | Definição |
|---|---|
| **exchange** | o roteador. Recebe a mensagem e decide para quais filas copiá-la. Não guarda nada. |
| **fila (queue)** | a caixa que **guarda** as mensagens até alguém consumir. |
| **binding** | a regra que liga uma exchange a uma fila. Sem binding, a exchange recebe a mensagem e a **descarta**. |
| **routing key** | a etiqueta que o produtor põe na mensagem; é o critério que o binding usa para decidir. |

Tipos de exchange:

| Tipo | Como decide |
|---|---|
| `direct` | entrega às filas cujo binding tem a routing key **exatamente igual** |
| `fanout` | copia para **todas** as filas ligadas, ignorando a routing key |
| `topic` | casa padrões: `pedido.*.criado`, `log.#` (`*` = uma palavra, `#` = várias) |
| `headers` | decide pelos cabeçalhos da mensagem, não pela routing key |

**A exchange padrão** merece parágrafo próprio, porque é o que este demo usa e é uma pegadinha
para quem lê o código sem saber:

```cpp
channel.publish("", nome, corpo);
//               │    │
//               │    └── routing key = "hello"
//               └─────── exchange VAZIA = a "default exchange"
```

A exchange de nome `""` é um caso especial do AMQP: toda fila é **automaticamente** ligada a
ela com um binding cuja routing key é o **próprio nome da fila**. Ou seja, publicar em `""`
com routing key `"hello"` entrega na fila chamada `"hello"` — sem que ninguém tenha criado
binding algum. É atalho de tutorial. Em aplicação real você declara a sua exchange e usa
routing keys que significam alguma coisa.

Dá para ver esse binding invisível com os próprios olhos (saída real desta máquina):

```
$ docker exec rabbitmq rabbitmqctl list_bindings
source_name  source_kind  destination_name  destination_kind  routing_key  arguments
             exchange     hello             queue             hello        []
   └── source_name VAZIO = a exchange padrão
```

### 4. Idempotente, e por que os dois lados declaram a fila

**Idempotente** = fazer a operação uma vez ou dez vezes dá o mesmo resultado final.
`declareQueue("hello")` é assim: se a fila não existe, cria; se já existe **com os mesmos
parâmetros**, não faz nada e devolve sucesso.

Por isso produtor **e** consumidor declaram a mesma fila. Não é redundância: é o que permite
subir os dois em qualquer ordem, sem que nenhum precise saber quem chegou primeiro.

> ⚠ Se a fila existir com parâmetros **diferentes** (durável vs não durável, por exemplo), o
> `declareQueue` falha com `PRECONDITION_FAILED` e **fecha o canal**. É o erro mais comum ao
> mudar a configuração de uma fila que já está em produção: não existe "alterar fila", só
> apagar e recriar.

### 5. ACK, etiqueta de entrega e redespacho

Quando o broker entrega uma mensagem, ele **não** a apaga. Ele a marca como "em voo" e espera
uma confirmação:

```
  broker ──entrega (delivery tag 1)──▶ consumidor
                                        │ processa...
  broker ◀──── basic.ack(1) ────────────┘   → agora sim a mensagem some da fila
```

| Termo | Definição |
|---|---|
| **delivery tag** (`uint64_t etiqueta`) | um número sequencial **por canal** que identifica aquela entrega. É o que você devolve no `ack`. |
| **ack** | "processei, pode apagar". |
| **nack / reject** | "não consegui". Com `requeue = true` a mensagem volta para a fila; com `false`, é descartada ou vai para a dead-letter. |
| **redelivered** | a flag que diz "você já viu esta mensagem antes" — o `bool redespachada` do callback. |
| **dead-letter queue** | uma fila configurada para receber o que foi rejeitado ou expirou, em vez de sumir. Serve para inspecionar falhas em vez de perdê-las. |

Se o consumidor morrer **antes** do `ack`, o broker percebe a conexão cair e **redespacha** a
mensagem para outro consumidor. Isso é recurso, não bug: garante que nada se perde no meio do
processamento. A regra que decorre disso: **`ack` depois de processar, nunca antes.**

> ⚠ O preço é a **entrega ao menos uma vez** (*at-least-once*): se o consumidor processar e
> morrer antes de mandar o `ack`, a mensagem chega de novo. Consumidor precisa ser idempotente
> — "cobrar o cartão" não pode acontecer duas vezes. A flag `redespachada` é a sua pista.

### 6. Round-robin e prefetch

Com dois consumidores na mesma fila, o broker distribui em **round-robin**: um para cada, na
ordem. Só que, por padrão, ele empurra mensagens para o consumidor **sem esperar os acks**.
Se o consumidor A pegar dez mensagens lentas e o B nenhuma, o round-robin fica injusto.

A correção é o **prefetch** (QoS): `channel.setQos(1)` diz "não me mande outra antes de eu dar
`ack` na atual". Aí o trabalho vai para quem está livre, e não para quem é a vez.

> ⚠ **Este demo não chama `setQos`.** Com um único consumidor não faz diferença; com vários,
> faria. É a primeira coisa a acrescentar ao transformar isto num *work queue* de verdade.

### 7. vhost

A URL `amqp://guest:guest@localhost:4672/` termina numa barra, e essa barra é o **vhost**
(`/`, o padrão). Um vhost é um namespace dentro do broker: filas, exchanges e permissões de um
vhost são invisíveis aos outros. Serve para separar ambientes ou times no mesmo servidor.
`amqp://user:senha@host:porta/producao` usaria o vhost `producao`.

### 8. Loop de eventos, `io_context` e handler

O AMQP-CPP é **assíncrono**: nenhuma chamada bloqueia esperando a rede. Você registra
*callbacks* — funções a serem chamadas depois — e entrega o controle a um **loop de eventos**.

```
   io.run()   ← a partir daqui o SEU código não está mais no comando
      │
      ├─ chegou byte no socket?     → decodifica → chama o seu callback
      ├─ o timer venceu?            → chama o seu callback
      └─ nada pendente?             → RETORNA, e o programa acaba
```

| Termo | Definição |
|---|---|
| **`boost::asio::io_context`** | o loop de eventos. `io.run()` fica processando eventos e só retorna quando não há mais trabalho pendente. |
| **handler** | no vocabulário do AMQP-CPP, o objeto que faz a ponte entre a biblioteca e o loop de eventos, e que recebe os eventos de ciclo de vida da conexão. |
| **callback** | a função que o loop chama quando algo acontece. Aqui aparecem em duas formas — lambda e método virtual — e a [seção sobre `handler.hpp`](#handlerhpp--por-que-uma-classe-e-não-lambdas) explica quando é cada uma. |

> ⚠ Depois de `io.run()`, escrever código na linha seguinte é inútil enquanto o loop roda.
> Tudo o que o programa faz de interessante está **dentro** dos callbacks. Se isso for novo,
> [`src/network/`](../network/README.md) apresenta o `io_context` com TCP puro, sem AMQP no
> meio.

---

## Por que estudar isto?

| Sem broker (HTTP síncrono direto) | Com broker (fila) |
|---|---|
| o receptor precisa estar no ar **agora** | a mensagem espera na fila |
| o emissor fica bloqueado até a resposta | publica e segue |
| pico de carga derruba o receptor | a fila absorve o pico |
| retentativa é código seu | redespacho é do broker |
| 1 emissor → 1 receptor | 1 emissor → N receptores, sem tocar no emissor |
| o emissor conhece o endereço do receptor | ninguém conhece ninguém |

E o que não é de graça:

| O que o broker cobra |
|---|
| mais uma peça de infraestrutura para operar, monitorar e atualizar |
| resposta deixa de ser imediata — quem precisa dela tem de implementar RPC sobre filas |
| entrega ao menos uma vez obriga o consumidor a ser idempotente |
| depurar fica mais difícil: o fluxo não aparece numa única pilha de chamadas |

Além do RabbitMQ, os conceitos se transferem quase intactos: Kafka, SQS, NATS e Redis Streams
mudam as garantias e o modelo de roteamento, mas produtor, fila, consumidor e confirmação são
as mesmas quatro peças.

---

## Estrutura de arquivos

```
src/rabbitmq/
├── README.md             ← este arquivo
├── handler.hpp           ← rabbit::Handler: os 6 eventos de ciclo de vida da conexão
├── producer.cpp          ← declara a fila e publica N mensagens; alvo rabbitMQ_producer
├── consumer.cpp          ← declara a mesma fila, consome e dá ack; alvo rabbitMQ_consumer
├── docker-compose.yml    ← sobe rabbitmq:3-management (host 4672 → contêiner 5672)
├── docker_rabbitmq.sh    ← caderno de comandos: subir o broker e inspecioná-lo
└── meson.build           ← os dois executáveis, ambos dependendo de handler.hpp
```

> ⚠ [`docker_rabbitmq.sh`](docker_rabbitmq.sh) é um **caderno de comandos**, não um script
> para executar inteiro: ele contém, entre outras linhas, um `purge_queue task_queue` de uma
> fila que este demo não cria. Copie a linha que interessa.

---

## `handler.hpp` — por que uma classe, e não lambdas

Esta é a lição central do subprojeto, e ela nasceu de um erro de compilação real.

A primeira versão deste código — e o README antigo, que a reproduzia — escrevia assim:

```cpp
AMQP::TcpConnection connection(&handler, address);
connection.onConnected([]{ ... });                  // ❌
connection.onReady([]{ ... });                      // ❌
connection.onError([](const char* m){ ... });       // ❌
```

E não compilava:

```
error: cannot convert 'main()::<lambda()>' to 'AMQP::TcpState*'
```

A mensagem é confusa porque `TcpConnection` **tem** métodos com esses nomes — só que são outra
coisa: fazem parte da máquina de estados interna e esperam um `TcpState*`. Os eventos de
**ciclo de vida da conexão** pertencem ao **handler**, e a forma de reagir a eles é **herdar e
sobrescrever métodos virtuais**.

Daí a regra de decisão, que vale muito além do AMQP-CPP:

| Use **lambda por evento** quando | Use **herança com virtual** quando |
|---|---|
| cada chamada tem o seu próprio retorno | há um conjunto **fixo e conhecido** de eventos |
| os callbacks são independentes entre si | os eventos compartilham **estado** |
| exemplo: `declareQueue().onSuccess(...)`, `consume().onReceived(...)` | exemplo: os 6 eventos da conexão, que precisam saber se já houve erro |

É por isso que, no mesmo arquivo, `channel.onReady(...)` recebe uma lambda e `Handler::onReady`
é um `override`. Não é inconsistência: são dois problemas diferentes. O mecanismo de despacho
virtual está em [`src/oop/demo_virtual.hpp`](../oop/demo_virtual.hpp), e
[`src/type_erasure/`](../type_erasure/README.md) discute quando `std::function` pode substituir
a herança — e quando não pode.

### Os seis estágios do ciclo de vida

```
 onAttached   objeto de conexão construído — ainda não há socket
      │
 onConnected  TCP estabelecido — ainda NÃO houve login AMQP
      │
 onReady      handshake AMQP concluído   ← só a partir daqui dá para publicar
      │
 onError      erro fatal; a conexão vai fechar
      │
 onClosed     fechamento limpo
      │
 onDetached   pode destruir o objeto
```

> ⚠ **Publicar em `onConnected` é cedo demais.** O TCP está de pé, mas o login AMQP não
> terminou e o canal ainda não existe. É a origem clássica do "publiquei e a mensagem sumiu".

### A armadilha do `override` que esquece a base

```cpp
void onConnected(AMQP::TcpConnection* c) override {
    AMQP::LibBoostAsioHandler::onConnected(c);   // ← NÃO é opcional
    fmt::print("[{}] onConnected — TCP estabelecido\n", rotulo_);
}
```

`LibBoostAsioHandler` faz **trabalho real** nesses métodos: registra o socket no `io_context`,
monitora leitura e escrita. Esquecer a chamada à base produz um programa que **compila,
conecta e nunca recebe nada** — o pior tipo de bug, porque parece funcionar.

É a armadilha de sobrescrever um virtual que **não é puro**: o compilador não reclama, porque
tecnicamente você o implementou. O padrão NVI (*Non-Virtual Interface*), discutido em
[`src/solid/`](../solid/README.md), existe em boa parte para evitar exatamente isto.

---

## Os demos, um a um

### Demo 1 · o consumidor — `consumer.cpp`

Comece por ele: sem consumidor no ar, o produtor publica e você não vê nada acontecer (embora
a mensagem esteja lá — veja [Como validar](#como-validar)).

O esqueleto mínimo, em ordem:

```cpp
boost::asio::io_context io;                       // 1. o loop de eventos
rabbit::Handler         handler{io, "CONSUMER"};  // 2. a ponte + os 6 eventos
AMQP::Address           address{url};             // 3. parse de amqp://user:senha@host:porta/vhost
AMQP::TcpConnection     connection{&handler, address};
AMQP::TcpChannel        channel{&connection};     // 4. canal dentro da conexão

channel.onReady([&channel] {                      // 5. só agora dá para falar AMQP
    channel.declareQueue("hello")
        .onSuccess([&channel](const std::string& nome, uint32_t, uint32_t) {
            channel.consume(nome)
                .onReceived([&channel](const AMQP::Message& msg,
                                       uint64_t etiqueta, bool redespachada) {
                    const std::string corpo{msg.body(), msg.bodySize()};
                    /* processa... */
                    channel.ack(etiqueta);        // 6. ack DEPOIS de processar
                });
        });
});

io.run();                                         // 7. entrega o controle ao loop
```

Note o encadeamento: `declareQueue()` devolve um *deferred*, e `.onSuccess(...)` registra o
que fazer quando a resposta do broker chegar. `consume()` só é chamado **dentro** desse
`onSuccess` — não adianta assinar uma fila que talvez ainda não exista.

O corpo da mensagem vem como `(ponteiro, tamanho)` e **não** é terminado em `\0`; por isso
`std::string corpo{msg.body(), msg.bodySize()}`. Construir com `std::string{msg.body()}`
leria memória além do buffer.

Saída real (broker no ar, consumidor com limite de 12 s, produtor rodando em seguida):

```
[CONSUMER] conectando em amqp://guest:guest@localhost:4672/
[CONSUMER] onAttached — objeto de conexão criado
[CONSUMER] onConnected — TCP estabelecido (AMQP ainda NÃO)
[CONSUMER] onReady — handshake AMQP concluído
[CONSUMER] canal pronto
[CONSUMER] fila 'hello' pronta (0 mensagem(ns), 0 consumidor(es))
[CONSUMER] recebido: "mensagem 1 de 5"
[CONSUMER] recebido: "mensagem 2 de 5"
[CONSUMER] recebido: "mensagem 3 de 5"
[CONSUMER] recebido: "mensagem 4 de 5"
[CONSUMER] recebido: "mensagem 5 de 5"
[CONSUMER] limite de 12 s atingido, encerrando
[CONSUMER] fim
```

Os seis primeiros passos são a máquina de estados do handler, na ordem. Os dois números do
`declareQueue` — `(0 mensagem(ns), 0 consumidor(es))` — são o estado da fila **no instante da
declaração**, informação que o próprio broker devolve.

> ⚠ **O timer não é decoração.** `io.run()` só retorna quando não há trabalho pendente, e um
> consumidor tem trabalho pendente para sempre. Sem o `steady_timer` que chama `io.stop()`, o
> programa nunca terminaria sozinho — nem quando não houvesse broker. Com ele, o demo vira um
> teste de fumaça com prazo. O mesmo padrão de timer aparece em
> [`src/network/demo_timer.hpp`](../network/demo_timer.hpp).

### Demo 2 · o produtor — `producer.cpp`

Mesmo esqueleto até o `channel.onReady`; a diferença está no que ele faz lá dentro:

```cpp
channel.declareQueue("hello")
    .onSuccess([&channel, &connection, quantas](const std::string& nome, uint32_t, uint32_t) {
        for (int i = 1; i <= quantas; ++i)
            channel.publish("", nome, fmt::format("mensagem {} de {}", i, quantas));
        connection.close();                       // publicou tudo, encerra
    });
```

Saída real (`./dist/bin/rabbitMQ_producer amqp://guest:guest@localhost:4672/ 5`):

```
[PRODUCER] conectando em amqp://guest:guest@localhost:4672/
[PRODUCER] onAttached — objeto de conexão criado
[PRODUCER] onConnected — TCP estabelecido (AMQP ainda NÃO)
[PRODUCER] onReady — handshake AMQP concluído
[PRODUCER] canal pronto
[PRODUCER] fila 'hello' pronta
[PRODUCER] publicado: "mensagem 1 de 5"
[PRODUCER] publicado: "mensagem 2 de 5"
[PRODUCER] publicado: "mensagem 3 de 5"
[PRODUCER] publicado: "mensagem 4 de 5"
[PRODUCER] publicado: "mensagem 5 de 5"
[PRODUCER] 5 mensagem(ns) enviada(s); fechando
[PRODUCER] onClosed — conexão encerrada
[PRODUCER] onDetached — seguro destruir
[PRODUCER] fim
```

Repare no par `onClosed` → `onDetached`: é o `connection.close()` sendo processado pelo loop.
Sem esse fechamento explícito, o produtor ficaria vivo até o timer de 15 s expirar.

> ⚠ **`publish` não confirma nada.** Ele só escreve no socket e retorna. Se o broker rejeitar
> a mensagem — fila cheia, permissão negada, exchange inexistente — o produtor **não fica
> sabendo**. A linha `[PRODUCER] publicado: ...` prova que a mensagem saiu, não que ela chegou.
> Para garantia real existem os *publisher confirms* (`channel.confirmSelect()`), que fazem o
> broker responder um `ack` por mensagem. É o mesmo trade-off de `send` versus `send + ack`
> de qualquer protocolo — ver [`src/network/`](../network/README.md).

### Demo 3 · sem broker no ar

Não é um demo separado no código, mas é o cenário mais provável na sua primeira execução, e o
programa foi escrito para tratá-lo bem. Rodando contra a porta 5672 (onde, com este compose,
não há ninguém):

```
$ ./dist/bin/rabbitMQ_consumer amqp://guest:guest@localhost:5672/ 8; echo "exit=$?"
[CONSUMER] conectando em amqp://guest:guest@localhost:5672/
[CONSUMER] onAttached — objeto de conexão criado
[CONSUMER] ⚠ erro de canal: Connection refused
[CONSUMER] ⚠ onError — Connection refused
[CONSUMER] onDetached — seguro destruir
[CONSUMER] limite de 8 s atingido, encerrando
[CONSUMER] fim
exit=1
```

Três coisas a aprender aqui:

1. **`onAttached` acontece, `onConnected` não.** O objeto de conexão foi construído; o socket
   nunca subiu. A máquina de estados diz exatamente onde parou.
2. **O erro aparece duas vezes** — uma no canal, outra no handler. O canal morre junto com a
   conexão que o carrega.
3. **O código de saída é 1**, porque `main` retorna `handler.erro() ? 1 : 0`. Isso torna o
   programa utilizável num script: dá para testar a presença do broker sem ler a saída.

---

## Durabilidade: o que este demo **não** garante

Há três níveis independentes de garantia, e este demo não liga nenhum:

| Nível | O que protege | Como se liga |
|---|---|---|
| **fila durável** | a fila sobrevive ao reinício do broker | `declareQueue("hello", AMQP::durable)` |
| **mensagem persistente** | a mensagem é gravada em disco, não só na RAM | `AMQP::Envelope` com `setDeliveryMode(2)` |
| **publisher confirms** | o produtor descobre se o broker aceitou | `channel.confirmSelect()` |

`declareQueue("hello")` sem argumentos cria uma fila **não durável**. Conferindo no broker
(saída real):

```
$ docker exec rabbitmq rabbitmqctl list_queues name durable auto_delete messages
name   durable  auto_delete  messages
hello  false    false        3
```

Consequência: **reiniciar o broker apaga a fila `hello` e tudo o que estiver nela.** Para um
demo isso é conveniente — cada execução começa limpa. Para um sistema real seria perda de
dados.

> ⚠ Os três níveis são independentes e todos necessários. Fila durável com mensagem não
> persistente: a fila volta vazia. Mensagem persistente em fila não durável: a fila não volta.
> E mesmo com os dois, sem *publisher confirms* o produtor não sabe se a gravação aconteceu
> antes da queda.

---

## Como validar

### Roteiro dos dois terminais

```bash
docker compose -f src/rabbitmq/docker-compose.yml up -d
docker compose -f src/rabbitmq/docker-compose.yml ps    # esperar "healthy"
make build
```

**Terminal A** (consumidor, 30 s de vida):

```bash
./dist/bin/rabbitMQ_consumer amqp://guest:guest@localhost:4672/
```

**Terminal B** (produtor, 5 mensagens):

```bash
./dist/bin/rabbitMQ_producer amqp://guest:guest@localhost:4672/ 5
```

Esperado: o terminal B imprime as 5 linhas `publicado:` e sai; o terminal A imprime as 5
linhas `recebido:` **na mesma ordem** e continua vivo até o timer. As duas saídas completas
estão nos demos 1 e 2 acima.

### A prova do desacoplamento

Este é o experimento que vale mais que a explicação. Rode **só o produtor**, com o consumidor
desligado:

```bash
./dist/bin/rabbitMQ_producer amqp://guest:guest@localhost:4672/ 3
docker exec rabbitmq rabbitmqctl list_queues name messages consumers
```

Saída real:

```
name   messages  consumers
hello  3         0
```

**Três mensagens esperando, zero consumidores.** O produtor terminou e saiu; as mensagens
continuam lá. Agora suba o consumidor:

```
[CONSUMER] fila 'hello' pronta (3 mensagem(ns), 0 consumidor(es))
[CONSUMER] recebido: "mensagem 1 de 3"
[CONSUMER] recebido: "mensagem 2 de 3"
[CONSUMER] recebido: "mensagem 3 de 3"
```

E a fila volta a zero:

```
name   messages  consumers
hello  0         0
```

### Inspecionando o broker

Os comandos abaixo estão em [`docker_rabbitmq.sh`](docker_rabbitmq.sh):

```bash
docker exec rabbitmq rabbitmqctl list_queues name messages consumers
docker exec rabbitmq rabbitmqctl list_exchanges name type
docker exec rabbitmq rabbitmqctl list_bindings
docker exec rabbitmq rabbitmqctl list_connections
```

`list_exchanges` mostra a exchange padrão como a linha de **nome vazio**, tipo `direct`, ao
lado das `amq.*` que o RabbitMQ cria sozinho:

```
name                 type
                     direct        ← a exchange padrão, sem nome
amq.fanout           fanout
amq.topic            topic
amq.direct           direct
amq.headers          headers
amq.match            headers
amq.rabbitmq.trace   topic
```

Pelo navegador: **http://localhost:14672**, `guest`/`guest`. A aba *Queues* mostra a fila
`hello` com o gráfico de mensagens em tempo real — rode o produtor com `1000` no `argv[2]` e
observe.

### Derrubando tudo

```bash
docker compose -f src/rabbitmq/docker-compose.yml down        # mantém o volume
docker compose -f src/rabbitmq/docker-compose.yml down -v     # apaga também os dados
```

---

## ⚠ O bug histórico deste subprojeto

O README anterior ensinava a API errada — `connection.onConnected([]{...})` com lambdas — que
**não compila** com a versão atual do AMQP-CPP. O código-fonte já tinha sido corrigido para
`rabbit::Handler`, mas o README continuava mostrando a versão quebrada, junto com o alvo
errado (`rabbitmq_producer`) e três portas incompatíveis entre si (5672 no texto, 4672 no
compose, 5673 num `docker run` que mapeava uma porta em que o RabbitMQ nem escuta).

A lição vale além deste diretório: **README que não é executado apodrece.** Todo comando e
toda saída deste arquivo foram rodados antes de escritos. O caso está catalogado em
[`src/ROADMAP.md`](../ROADMAP.md), junto com os outros alvos que estavam latentes.

---

## Referências

Dentro deste repositório:

- [`src/network/`](../network/README.md) — `io_context`, TCP síncrono e assíncrono, timers: a
  camada logo abaixo do AMQP-CPP
- [`src/oop/demo_virtual.hpp`](../oop/demo_virtual.hpp) — como o despacho virtual funciona, que
  é o mecanismo do `rabbit::Handler`
- [`src/type_erasure/`](../type_erasure/README.md) — `std::function` versus herança, a outra
  metade da decisão de callback
- [`src/solid/`](../solid/README.md) — NVI e a armadilha do virtual não puro
- [`src/concurrency/`](../concurrency/README.md) — filas produtor/consumidor **dentro** de um
  processo; o mesmo padrão, sem rede
- [`src/errors/`](../errors/README.md) — erro como valor versus exceção, que é o estilo dos
  callbacks `onError`
- [`handler.hpp`](handler.hpp) · [`producer.cpp`](producer.cpp) ·
  [`consumer.cpp`](consumer.cpp) — os três fontes, comentados

Fora dele:

- RabbitMQ Tutorials — https://www.rabbitmq.com/tutorials — os tutoriais 1 (*Hello World*) e 2
  (*Work Queues*) correspondem exatamente a este demo
- Especificação AMQP 0-9-1 — https://www.rabbitmq.com/resources/specs/amqp0-9-1.pdf
- AMQP-CPP (Copernica) — https://github.com/CopernicaMarketingSoftware/AMQP-CPP
- Boost.Asio — https://www.boost.org/doc/libs/release/doc/html/boost_asio.html
