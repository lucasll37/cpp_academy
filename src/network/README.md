# network — Sockets, Asio e o Modelo Assíncrono

## O que é este projeto?

Cinco demos executáveis de **programação de rede** em C++ com a biblioteca **Asio**
(*standalone*, sem Boost): TCP síncrono, TCP assíncrono, UDP, timers e HTTP/1.1 escrito à mão
sobre um socket cru.

```bash
make build && ./dist/bin/network        # pipeline completo do repositório

source build/conanbuild.sh              # ou: recompilar só este alvo
meson compile -C build network && ./dist/bin/network
```

> **Precisa de internet?** Os demos **1 a 4 rodam offline**: sobem servidor *e* cliente dentro
> do próprio processo, conversando por `127.0.0.1`. Só os demos **5.1 e 5.2** saem para a
> internet (`httpbin.org`); sem rede eles imprimem `sem conectividade` e o programa segue até
> o fim.

---

## Antes de começar: os conceitos que você precisa

Nada aqui presume mais que C++ básico (funções, classes, lambdas, `std::thread`). Todo termo
usado no resto do README é definido nesta seção.

### 1. O que é um socket

Um **socket** é uma "tomada de rede": um objeto do sistema operacional pelo qual dois programas
trocam bytes. Você escreve bytes de um lado, eles saem do outro. Partir isso em pacotes, mandar
pela placa de rede, retransmitir o que se perdeu e remontar na ordem é trabalho do **kernel** —
o socket é a alça que o seu programa segura.

```
   processo A (cliente)                        processo B (servidor)
  ┌────────────────────┐                      ┌────────────────────┐
  │  asio::write(...)  │                      │  asio::read(...)   │
  └─────────┬──────────┘                      └─────────▲──────────┘
            │ socket                            socket  │
  ══════════▼═══════════════════════════════════════════╧══════════
       kernel: pilha TCP/IP  →  placa de rede  →  ...  →  kernel
```

Dois números identificam a ponta de uma conversa: o **endereço IP** (qual máquina —
`127.0.0.1` é sempre "esta máquina") e a **porta**, um número de 0 a 65535 que diz *qual
programa* naquela máquina. Portas abaixo de 1024 exigem privilégio; 80 é HTTP, 443 é HTTPS, 53
é DNS. Os demos usam 12345–12349, longe de qualquer serviço real. O par `(IP, porta)` é o que o
Asio chama de **endpoint** (`tcp::endpoint`).

### 2. Cliente, servidor e o `acceptor`

A assimetria é toda a diferença: o **servidor** publica um endpoint e **espera**; o **cliente**
conhece o endpoint e **conecta**. Em C isso são quatro chamadas (`socket`, `bind`, `listen`,
`accept`); o Asio junta as três primeiras no construtor de um **acceptor** — literalmente "o
aceitador de conexões". O acceptor **não** transporta dados: só produz sockets novos, um por
cliente que chega.

### 3. Mapa: sockets BSD (C) ↔ Asio (C++)

| C / POSIX | Asio | O que faz |
|---|---|---|
| `socket()+bind()+listen()` | construtor de `tcp::acceptor` | reserva a porta e escuta |
| `accept()` | `acceptor.accept(sock)` / `async_accept` | devolve um socket por cliente |
| `connect()` | `asio::connect(sock, eps)` | cliente toma a iniciativa |
| `send()` / `recv()` | `asio::write` / `asio::read`, `read_until` | transfere bytes |
| `close()` | destrutor de `tcp::socket` (RAII) | fecha — não há `close()` a esquecer |
| `getaddrinfo()` | `tcp::resolver` | nome → endereços |
| `sendto()` / `recvfrom()` | `send_to` / `receive_from` | UDP: endereço a cada datagrama |
| `errno` | `asio::error_code` | código de erro, sem variável global |

### 4. Nome → endereço: DNS e o `resolver`

Sockets não entendem `"httpbin.org"`; entendem `54.156.x.y`. Traduzir nome em endereço é
**resolução de nome**, e o serviço que faz isso na internet é o **DNS**. No Asio,
`resolver.resolve("localhost", "80")` devolve uma **lista** — um nome pode resolver para vários
endereços (IPv4 e IPv6, várias máquinas), e é por isso que `asio::connect(socket, endpoints)`
tenta um por um até algum funcionar.

### 5. Bloquear uma thread

"Bloquear" quer dizer: a thread **para de executar instruções** e o kernel a tira da CPU até o
dado chegar. Ela não gasta CPU — mas também não faz mais nada.

```
SÍNCRONO (bloqueante)                ASSÍNCRONO
──────────────────────               ──────────────────────────────────
t0  asio::read(sock, buf)            t0  async_read(sock, buf, handler)
      thread PARADA                        retorna IMEDIATAMENTE
      ... 40 ms ...                  t0  a thread faz outra coisa
t1  read retorna, 12 bytes           t1  io.run() chama handler(ec, 12)
```

Com o modelo síncrono, atender 1000 conexões pede 1000 threads — cada uma com megabytes de
*stack* reservado e um lugar na fila do escalonador. Com o assíncrono, 1000 conexões são
**1 thread** e 1000 operações registradas.

### 6. `io_context`, handler e o ciclo de vida do `run()`

- **handler** (*callback*): a lambda que você entrega ao Asio para ser chamada *depois*, quando
  a operação completar. Todo handler de I/O recebe ao menos um `asio::error_code`.
- **`io_context`**: o motor. Guarda as operações pendentes, conversa com o `epoll`/`kqueue`/IOCP
  do sistema e **chama os handlers**. Todo objeto de I/O do Asio (socket, acceptor, resolver,
  timer) nasce ligado a um `io_context`.
- **event loop**: o laço dentro de `io_context::run()` — "espere algo ficar pronto, chame o
  handler, repita". Ele **retorna** assim que não sobra trabalho pendente.

Três consequências diretas:

1. Registrar `async_wait` e **nunca** chamar `run()` = o handler **nunca** roda.
2. Chamar `run()` sem nada registrado = ele retorna na hora.
3. Quando todos os handlers terminam, `run()` retorna — mesmo que você fosse mandar mais
   trabalho depois. Para impedir isso existe o **work guard** (`asio::make_work_guard(io)`): um
   objeto que declara "ainda há trabalho, não retorne"; `guard.reset()` o libera. Sem ele, um
   pool de threads rodando `io.run()` morreria no primeiro instante de ociosidade.

### 7. O menor programa Asio possível

Este arquivo é o esqueleto de **todos** os demos assíncronos:

```cpp
#include <asio.hpp>
#include <chrono>
#include <cstdio>

int main() {
    asio::io_context io;                                     // 1. o motor
    asio::steady_timer timer(io, std::chrono::seconds(1));   // 2. objeto de I/O

    timer.async_wait([](const asio::error_code& ec) {        // 3. registra o handler
        if (!ec) std::puts("2) o handler rodou — 1 segundo depois");
    });

    std::puts("1) async_wait ja retornou; nada aconteceu ainda");
    io.run();                                                // 4. o event loop
    std::puts("3) run() retornou: nao ha mais trabalho pendente");
}
```

```bash
ASIOP=$(sed -n 's|^prefix=||p' build/asio.pc)
g++ -std=c++20 -DASIO_STANDALONE -I$ASIOP/include minimo.cpp -o /tmp/minimo -lpthread && /tmp/minimo
```

Saída **medida** — a linha 1 sai antes da 2, e o programa leva 1 segundo:

```
1) async_wait ja retornou; nada aconteceu ainda
2) o handler rodou — 1 segundo depois
3) run() retornou: nao ha mais trabalho pendente
```

É o modelo assíncrono inteiro: registrar é instantâneo, executar é responsabilidade do `run()`.

### 8. Os três buffers do Asio

| O que é | Para que serve |
|---|---|
| `asio::buffer(x)` | **Função.** Envolve memória que você **já tem** (`std::string`, `std::array`) numa view `{ponteiro, tamanho}`. Não aloca, não copia, não é dona de nada. |
| `asio::streambuf` | **Classe.** Buffer que **cresce sozinho**. É o que `read_until` precisa: ninguém sabe de antemão quantos bytes chegam até o delimitador. |
| `asio::dynamic_buffer(s)` | **Adaptador** que faz um `std::string`/`std::vector` seu se comportar como streambuf. Alternativa moderna; este projeto usa `streambuf`. |

> **⚠ `asio::buffer` sobre um literal inclui o `\0`.** Medido: `asio::buffer("BYE\n").size()`
> devolve **5**, não 4. Por isso `demo_tcp_sync.hpp` escreve `asio::buffer("BYE\n", 4)` com o
> tamanho explícito — o byte nulo extra faria a comparação `linha == "BYE\n"` falhar do outro
> lado.

### 9. `read_until`, delimitadores e o problema de *framing*

TCP é um **fluxo de bytes**, não uma sequência de mensagens. Se o cliente escreve `"oi\n"` e
depois `"tchau\n"`, o servidor pode receber `"oi\ntch"` numa leitura e `"au\n"` na outra: TCP
não preserva fronteiras de envio, e delimitar mensagens (*framing*) é problema seu.

A solução mais simples é um **delimitador**. `asio::read_until(socket, buf, '\n')` lê até
encontrar `'\n'` e devolve `n`, a posição logo após ele. O HTTP usa `\r\n` como fim de linha e
uma linha **vazia** (`\r\n\r\n`) como fim dos cabeçalhos — herança dos protocolos de texto da
ARPANET, fixada na RFC 9112.

> **⚠ `read_until` pode ler MAIS que a linha.** Ele leva o que o kernel tiver disponível; se
> duas mensagens chegaram juntas, ambas ficam no `streambuf`. É por isso que o servidor de 1.1
> faz `buffers_begin(buf.data()) + n` (só até o delimitador) e `buf.consume(n)` (descarta só o
> consumido). O *cliente* do mesmo demo usa `buffers_end` e leva o buffer inteiro — funciona ali
> porque o servidor responde uma linha por vez, mas seria um bug num servidor real.

### 10. EOF, exceção e `error_code`

**EOF** (*end of file*) é o sinal de "acabou": num socket, é o outro lado ter fechado a sua
metade da conexão. O Asio reporta como `asio::error::eof` — um `error_code` como qualquer
outro, mas que quase sempre significa encerramento normal, não falha. Daí o padrão que aparece
em quase todos os demos: `if (ec && ec != asio::error::eof) { /* isto sim é erro */ }`.

Toda operação do Asio tem duas assinaturas:

```cpp
asio::connect(socket, endpoints);        // falhou → lança asio::system_error
asio::connect(socket, endpoints, ec);    // falhou → preenche ec, não lança
```

`asio::error_code` carrega um **valor** (`ec.value()`, o `errno` do sistema), uma **categoria**
(`ec.category().name()`, o "namespace" daquele número) e `ec.message()`. Falha de rede é rotina,
não excepcional — em servidor, o estilo `error_code` costuma ser o certo. Ver
[`src/errors/`](../errors/).

### 11. `strand`

Se `io.run()` roda em **várias threads**, dois handlers da mesma conexão podem executar em
paralelo — *data race* sobre o estado da conexão. Um **strand** é uma fila que garante: os
handlers postados nele **nunca** rodam ao mesmo tempo. É a exclusão mútua do Asio, sem `mutex`,
sem deadlock e sem thread parada esperando.

### 12. TCP e UDP

| | TCP | UDP |
|---|---|---|
| Modelo | fluxo de bytes, com conexão | datagramas independentes |
| Entrega | garantida e **em ordem** | pode perder, duplicar, desordenar |
| Antes de enviar | *three-way handshake* | nada |
| Fronteira de mensagem | não existe (§9) | um `receive_from` = um datagrama |
| Cabeçalho | 20+ bytes | 8 bytes |
| Usado por | HTTP, SSH, bancos de dados | DNS, jogos, voz, vídeo, telemetria |

O **three-way handshake** abre uma conexão TCP: o cliente manda `SYN`, o servidor responde
`SYN+ACK`, o cliente confirma com `ACK`. Custa uma ida-e-volta completa **antes** do primeiro
byte útil — é esse custo que o UDP não paga, e é por isso que o DNS usa UDP. Em UDP,
`connect()` **não** faz handshake: apenas grava o endereço remoto padrão no socket (demo 3.3).

### 13. ⚠ `ASIO_STANDALONE` e a violação de ODR silenciosa

Todo cabeçalho deste projeto começa com `#ifndef ASIO_STANDALONE / #define ASIO_STANDALONE /
#endif` antes de `#include <asio.hpp>`. O macro diz ao Asio para usar `std::error_code` em vez
de `boost::system::error_code`, e o `#ifndef` não é zelo excessivo: o pacote Asio do Conan
**já** define o macro na linha de compilação — confira em `build/asio.pc`, campo
`Cflags: -I... -DASIO_STANDALONE`. Redefini-lo com o mesmo valor já dispara
`warning: "ASIO_STANDALONE" redefined`; com valor **diferente** seria pior: duas unidades de
tradução enxergariam tipos incompatíveis com o mesmo nome — a violação silenciosa da *One
Definition Rule* documentada em [`src/linkage/demo_odr.hpp`](../linkage/demo_odr.hpp).

---

## Por que estudar isto?

| Sem entender sockets | Entendendo |
|---|---|
| "o cliente HTTP travou" é um mistério | você sabe se travou no DNS, no `connect` ou esperando bytes |
| framing por sorte: às vezes a mensagem chega inteira | delimitador ou prefixo de tamanho, por decisão |
| uma thread por conexão; o servidor cai em 800 clientes | 1 thread, N conexões, custo previsível |
| a biblioteca de rede é uma caixa preta | libcurl, gRPC, Redis, Postgres — tudo é isto por baixo |

1. **Está debaixo de tudo.** Todo backend, driver de banco, RPC e broker é um socket com um
   protocolo por cima. [`src/http-client/`](../http-client/), [`src/database/`](../database/) e
   [`src/rabbitmq/`](../rabbitmq/) são três camadas construídas sobre o que está aqui.
2. **O modelo assíncrono é a resposta de escala.** É a conta da §5 que separa um servidor de
   brinquedo de um servidor real — e o demo 2.1 a demonstra com três clientes simultâneos
   atendidos por **uma** thread.
3. **Asio é o padrão de fato.** Base do *Networking TS* e da proposta de `std::net`; a mesma API
   aparece como `boost::asio` em metade do C++ industrial.

---

## Estrutura de arquivos

```
src/network/
├── main.cpp             ← ponto de entrada: roda os 5 demos em sequência
├── demo_tcp_sync.hpp    ← 1 · echo server/client bloqueante, resolver, error_code
├── demo_tcp_async.hpp   ← 2 · servidor multi-conexão em 1 thread, post, strand
├── demo_udp.hpp         ← 3 · datagramas: síncrono, assíncrono, "conectado"
├── demo_timer.hpp       ← 4 · steady_timer, cancelamento, heartbeat, work_guard
├── demo_http_raw.hpp    ← 5 · HTTP/1.1 escrito e parseado à mão sobre TCP
├── meson.build          ← alvo `network`, com fmt_dep e asio_dep
└── README.md
```

---

## Os demos, um a um

### Demo 1 — TCP síncrono (`demo_tcp_sync.hpp`)

**O problema:** fazer dois programas trocarem texto, do jeito mais simples possível.
**A decisão de projeto:** servidor **e** cliente vivem no mesmo processo — o servidor numa
`std::thread`, o cliente na thread principal, conversando por `127.0.0.1:12345`. É o que torna
o demo executável offline e reprodutível, sem abrir dois terminais.

**1.1 — Echo server/client.** O servidor, reduzido ao essencial:

```cpp
tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), porta));  // socket+bind+listen
tcp::socket socket(io);
acceptor.accept(socket);                                // BLOQUEIA até um cliente chegar

asio::streambuf buf;
std::size_t n = asio::read_until(socket, buf, '\n');    // BLOQUEIA até ver '\n'
std::string linha(asio::buffers_begin(buf.data()),
                  asio::buffers_begin(buf.data()) + static_cast<std::ptrdiff_t>(n));
buf.consume(n);
asio::write(socket, asio::buffer(linha));               // ecoa de volta
```

O cliente resolve `127.0.0.1`, conecta e manda três linhas; a terceira é `"FIM\n"`, que o
servidor responde com `"BYE\n"` antes de encerrar. Saída **medida** (a intercalação das duas
threads varia entre execuções):

```
  [client] enviado: 'olá servidor
'  [server] cliente conectado de 127.0.0.1:52810
  [server] recebido: 'olá servidor
```

> **⚠ A saída sai "quebrada" de propósito:** as linhas ecoadas já terminam em `\n` e os
> `fmt::print` das duas threads não são sincronizados. Não é bug de rede — é a intercalação de
> `stdout` de [`src/concurrency/`](../concurrency/).
>
> **⚠ A porta 12345 pode estar ocupada:** o construtor do `acceptor` lança então
> `asio::system_error` com `Address already in use`. Servidores reais ligam
> `acceptor.set_option(tcp::acceptor::reuse_address(true))` para reabrir uma porta em
> `TIME_WAIT`.

**1.2 — Resolução de nomes.** Saída **medida** nesta máquina: `'localhost:80' resolve para:
127.0.0.1 (IPv4)`. Numa máquina com IPv6 habilitado para `localhost`, `::1` apareceria também
— por isso `resolve` devolve uma **lista**.

**1.3 — `error_code` em vez de exceção.** Conecta na porta 9999, que ninguém escuta:

```
  connect falhou (esperado — porta 9999 fechada): Connection refused
  categoria: 'asio.system', valor: 111
```

O **111** é o `ECONNREFUSED` do Linux; `asio.system` é a categoria que dá sentido ao número.
`Connection refused` quer dizer que a máquina existe e respondeu com um `RST` — bem diferente
de um *timeout*, em que não vem resposta alguma.

---

### Demo 2 — TCP assíncrono (`demo_tcp_async.hpp`)

**O problema:** o demo 1 atende **um** cliente por vez; enquanto o servidor está parado em
`read_until`, o segundo cliente espera. A cadeia de handlers substitui o `while (true)`:

```
async_accept → handler → async_read_until → handler → async_write → handler
                                  ▲                                     │
                                  └─────────────────────────────────────┘
```

**Tempo de vida, e por que `shared_ptr`.** Uma operação assíncrona fica pendente por tempo
indeterminado; se o objeto que contém socket e buffer for destruído antes do handler rodar, o
handler acessa memória morta. A solução canônica do Asio é herdar de
`std::enable_shared_from_this` e cada handler capturar um `shared_ptr` para si mesmo:

```cpp
asio::async_read_until(socket_, buf_entrada_, '\n',
    [self = shared_from_this()](asio::error_code ec, std::size_t bytes) {
        self->ao_ler(ec, bytes);
    });
```

A contagem de referências vira o gerenciador de tempo de vida da conexão: quando o handler
decide **não** re-registrar (erro, EOF ou `"FIM\n"`), a última referência morre, a `Sessao` é
destruída e o socket fecha no destrutor, por RAII. Não há `delete` em lugar nenhum.

**2.1 — Servidor multi-conexão com uma thread.** `ServidorAsync` chama `async_accept` e,
**dentro** do handler, cria a sessão e re-registra outro `async_accept`. Saída **medida**:

```
  [sessão 1] recebido: 'hello do cliente 1
'  [sessão 2] cliente conectado de 127.0.0.1:54274
  [cli 1] 'hello do cliente 1' → 'ECHO: hello do cliente 1'
  [sessão 3] cliente conectado de 127.0.0.1:54288
```

Os números de sessão aparecem **intercalados** — a 2 nasce enquanto a 1 ainda conversa. É a
prova de que uma única thread (`std::thread io_thread([&io]{ io.run(); })`) atende as três.

**2.2 — `asio::post`.** Enfileira uma função para rodar **na** thread do `io_context`, de forma
thread-safe: é o jeito idiomático de passar trabalho ao event loop sem `mutex`. O demo mantém
`io.run()` vivo com um `work_guard` e o libera com `guard.reset()` — o mecanismo da §6.

**2.3 — `strand`.** Duas threads rodam o mesmo `io_context` e cinco handlers incrementam o
**mesmo** `int`. Sem proteção seria uma *data race*; com `asio::post(strand, ...)` o resultado
medido é `resultado final = 5 (esperado 5, sem race condition)`.

> **⚠ O strand não protege a variável, protege os handlers.** Se outro código tocar em
> `contador` fora do strand, a race volta.

---

### Demo 3 — UDP (`demo_udp.hpp`)

**O problema:** handshake e retransmissão custam latência. Para DNS, jogos e telemetria, um
pacote perdido é preferível a um pacote atrasado. Não há `acceptor`, `accept` nem conexão: um
`udp::socket` ligado a uma porta já recebe de qualquer um.

**3.1 — Echo UDP síncrono:**

```cpp
udp::socket socket(io, udp::endpoint(udp::v4(), porta));
std::array<char, 256> buf{};
udp::endpoint remetente;                                  // preenchido pela chamada
std::size_t n = socket.receive_from(asio::buffer(buf), remetente);
socket.send_to(asio::buffer("ECHO:" + std::string(buf.data(), n)), remetente);
```

A diferença conceitual está no `remetente`: sem conexão, **cada** datagrama traz o endereço de
quem mandou, e você precisa dele para responder. O cliente usa `udp::endpoint(udp::v4(), 0)` —
porta **0** significa "sistema, escolha uma livre".

> **⚠ Buffer pequeno perde bytes, em silêncio.** Um datagrama maior que os 256 bytes do array
> é **truncado**; o excedente não fica para a próxima leitura, como aconteceria em TCP.
>
> **⚠ Estes demos só funcionam porque são locais.** Em `127.0.0.1` nenhum pacote se perde. Numa
> rede real o `receive_from` bloqueante do cliente esperaria **para sempre** por uma resposta
> que talvez nunca venha; um cliente UDP de verdade precisa de um `steady_timer` de timeout que
> chame `socket.cancel()` — os demos 4.2 e 4.3 mostram as duas peças.

**3.2 — UDP assíncrono.** `async_receive_from` com um `std::function<void()> iniciar` que se
re-registra dentro do próprio handler — o padrão de cadeia do demo 2 aplicado a datagramas.
Após dois datagramas ele para, o `io_context` esvazia e `run()` retorna.

**3.3 — UDP "conectado".** `sock.connect(...)` **não** manda pacote nenhum: grava o endereço
remoto no socket. Depois disso valem `sock.send(buf)` e `sock.receive(buf)` sem repetir o
destino, e o kernel passa a **descartar** datagramas de qualquer outro endereço — filtro grátis.

---

### Demo 4 — Timers (`demo_timer.hpp`)

**O problema:** `std::this_thread::sleep_for` bloqueia a thread inteira; num servidor
assíncrono isso congela todas as conexões. O Asio integra o tempo ao mesmo event loop: um timer
que expira é só mais um handler. `steady_timer` usa `std::chrono::steady_clock`, um relógio
**monotônico** que nunca anda para trás — um `system_clock` poderia pular com um ajuste de NTP
e fazer o timer disparar cedo, ou nunca.

| Sub-demo | O que mostra | Saída medida |
|---|---|---|
| 4.1 `wait()` | a versão bloqueante, para comparar | `aguardando 30ms... / timer expirou` |
| 4.2 `async_wait()` | registrar ≠ executar | ver abaixo |
| 4.3 `cancel()` | cancelamento | `timer cancelado antes de expirar ✓` |
| 4.4 periódico | heartbeat por re-registro | `tick #1 … tick #4` |
| 4.5 `work_guard` | pool de 3 threads, 6 tarefas | `6 tarefas executadas por 3 threads` |

A saída **medida** de 4.2 é a demonstração literal da §6:

```
  async_wait retornou imediatamente
  chamando io_context.run()...
  handler: timer expirou!
  io_context.run() retornou — nenhum trabalho pendente
```

**Cancelamento (4.3):** `timer.cancel()` **não** faz o handler desaparecer — ele é chamado
assim mesmo, com `ec == asio::error::operation_aborted`. Todo handler assíncrono precisa tratar
esse código: é o "sua operação foi abortada, não confie nos dados".

**Periódico (4.4):** não existe "timer que repete" no Asio. O padrão é o handler reconfigurar
(`expires_after`) e re-registrar (`async_wait`) a si mesmo; quando para, o `io_context` esvazia
e `run()` retorna — é assim que o demo termina sozinho após 4 ticks.

> **⚠ Aqui `std::function` é necessário:** uma lambda não pode se referenciar dentro do próprio
> corpo (o nome ainda não existe durante a inicialização). Declarar
> `std::function<void(asio::error_code)> tick;` antes e atribuir a lambda depois resolve.

---

### Demo 5 — HTTP sobre TCP cru (`demo_http_raw.hpp`)

**O problema:** HTTP parece mágico até você ver que é **texto** sobre TCP.

```
GET /get HTTP/1.1\r\n          ← linha de requisição: método, caminho, versão
Host: httpbin.org\r\n          ← cabeçalhos: "Nome: valor", um por linha
Connection: close\r\n
\r\n                            ← linha VAZIA = acabaram os cabeçalhos

HTTP/1.1 200 OK\r\n            ← linha de status: versão, código, texto
Content-Type: application/json\r\n
\r\n
{ "args": {}, ... }            ← corpo
```

`Connection: close` pede que o servidor feche a conexão ao terminar, o que dá ao cliente um
sinal de fim de corpo grátis: leia até o **EOF** (§10). Sem isso seria preciso interpretar
`Content-Length` ou *chunked transfer encoding*.

**5.1 — GET síncrono.** O corpo do demo é literalmente isto:

```cpp
asio::write(socket, asio::buffer(req));
asio::streambuf buf;
asio::read(socket, buf, asio::transfer_all(), ec);       // lê até EOF
if (ec && ec != asio::error::eof) { /* erro de verdade */ }
```

O parser lê a linha de status com `sl >> ver >> r.status`, acumula cabeçalhos até a linha vazia
e joga o resto no corpo. Saída **medida**: `status: 200 OK`, seguida dos 200 primeiros
caracteres do JSON que o httpbin devolve (`"args"`, `"headers"`, `"Host": "httpbin.org"`…).

> **⚠ `ver` é descartado de propósito.** Ela recebe a string `"HTTP/1.1"` inteira, prefixo
> incluído: imprimir `"HTTP/{} {} {}"` com ela produziria `HTTP/HTTP/1.1 200 OK`. O código
> imprime só `{} {}`, com `status` e `status_text`.
>
> **⚠ Porta 80, sem TLS.** HTTPS exigiria `asio::ssl::stream`, validação de certificado e SNI —
> a razão de o subprojeto vizinho usar libcurl.

**5.2 — GET assíncrono.** A mesma requisição com a cadeia `async_resolve → async_connect →
async_write → async_read`, cada etapa dentro do handler da anterior. Saída **medida**:
`[async] 200 OK — body: { "uuid": "224cf258-9a19-4846-822a-d9fc083cecd7" }`.

Repare no formato do fonte: quatro lambdas aninhadas, cada uma recuando mais. Isso é o
**callback hell**, a crítica clássica ao modelo por callbacks. Duas saídas: o padrão
classe-com-métodos do demo 2 (que achata a cadeia em `ler()`/`ao_ler()`/`escrever()`), ou
**corrotinas** — funções que podem *suspender* no meio e *retomar* depois, escritas como se
fossem síncronas. O Asio as suporta via `asio::awaitable` e `co_spawn`; o mecanismo da
linguagem está em [`src/coroutines/`](../coroutines/).

**5.3 — O comparativo.** Lista o que **falta** aqui para ser um cliente HTTP de produção: TLS,
redirecionamentos, *chunked encoding*, keep-alive, gzip, cookies, autenticação, HTTP/2,
timeouts e retry. A conclusão é a divisão de trabalho entre os subprojetos: TCP cru para
entender o protocolo (aqui), libcurl para produção ([`src/http-client/`](../http-client/)).

---

## Como validar

```bash
make build && ./dist/bin/network
```

A última linha, em verde, deve ser `✓ Todos os demos de rede concluídos!`. Antes dela, cinco
cabeçalhos em caixa (`1 · TCP Síncrono` … `5 · HTTP sobre TCP raw`) e as subseções numeradas
`1.1`–`1.3`, `2.1`–`2.3`, `3.1`–`3.3`, `4.1`–`4.5`, `5.1`–`5.3`.

Compilar isolado, com todos os avisos, e rodar sob o detector de *data race* — o teste que
importa aqui, já que quatro dos cinco demos usam threads:

```bash
ASIOP=$(sed -n 's|^prefix=||p' build/asio.pc)
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)

g++ -std=c++20 -Wall -Wextra -Wpedantic -DASIO_STANDALONE \
    -I$ASIOP/include -I$FMTP/include \
    src/network/main.cpp -o /tmp/net -L$FMTP/lib -lfmtd -lpthread

g++ -std=c++20 -O1 -g -fsanitize=thread -DASIO_STANDALONE \
    -I$ASIOP/include -I$FMTP/include \
    src/network/main.cpp -o /tmp/net_tsan -L$FMTP/lib -lfmtd -lpthread && /tmp/net_tsan
```

Estado **medido** desta árvore: compila **sem nenhum aviso** com `-Wall -Wextra -Wpedantic` e
roda **limpo sob ThreadSanitizer** (zero ocorrências de `WARNING: ThreadSanitizer`). A
compilação com TSan emite avisos `-Wtsan` sobre `atomic_thread_fence` vindos dos cabeçalhos do
Asio; são do Asio, não deste código, e não indicam race.

## Referências

- [`src/http-client/`](../http-client/) — o mesmo HTTP com libcurl: o contraste "cru vs biblioteca"
- [`src/concurrency/`](../concurrency/) — `thread`, `mutex`, `future`: as ferramentas que sobem
  cliente e servidor no mesmo processo
- [`src/concurrency2/`](../concurrency2/) — modelo de memória e data race, o que o `strand` evita
- [`src/coroutines/`](../coroutines/) — a saída do *callback hell* do demo 5.2
- [`src/errors/`](../errors/) — `error_code` vs exceção, a escolha do demo 1.3
- [`src/linkage/`](../linkage/) — ODR e macros de compilação, a nota sobre `ASIO_STANDALONE`
- [Asio 1.28 — documentação e tutoriais](https://think-async.com/Asio/asio-1.28.0/doc/) — a
  versão usada aqui (veja `conanfile.py`)
- RFC 9293 — *Transmission Control Protocol* (substitui a clássica RFC 793)
- RFC 768 — *User Datagram Protocol* (três páginas; vale ler inteira)
- RFC 9110 — *HTTP Semantics* · RFC 9112 — *HTTP/1.1 Message Syntax*
- RFC 1035 — *Domain Names: Implementation and Specification* (DNS)
