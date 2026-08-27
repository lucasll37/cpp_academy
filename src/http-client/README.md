# http-client — HTTP e JSON em C++, do protocolo ao objeto

## O que é este projeto?

Um cliente **HTTP** de verdade, escrito em ~120 linhas de C++ sobre a **libcurl**, que baixa **JSON** de uma API
pública e o transforma em `struct` C++ com a **nlohmann/json**. Quatro demos rodam em sequência: um recurso único, uma
coleção com filtro, um cabeçalho customizado e um erro 404.

```bash
make build && ./dist/bin/http_client      # pipeline completo do repositório

source build/conanbuild.sh                # ou: recompilar só este alvo
meson compile -C build http_client && ./dist/bin/http_client
```

> ⚠ **Precisa de internet.** Os quatro demos falam com `https://jsonplaceholder.typicode.com`, uma API pública
> de testes. Sem rede, a primeira requisição lança `http::HttpError` com a mensagem da libcurl (por exemplo
> `Could not resolve host`) e o programa retorna `1`.

---

## Antes de começar: tudo o que este README vai usar

Esta seção define **todo** termo que aparece adiante. Não se pressupõe nada além de C++ básico:
`struct`, função, `std::string`, `std::map`, exceção. Se algum título já for familiar, pule.

### 1. O que é HTTP

**HTTP** é um protocolo de **texto**: um programa **cliente** manda um bloco de texto para um programa
**servidor** e recebe outro bloco de texto de volta. É só isso — não há mágica binária. Cada requisição é
independente da anterior (o protocolo é **sem estado**).

```
        cliente (seu programa)                    servidor (jsonplaceholder)
                 │  ── requisição (texto) ──────────────────▶ │
                 │  ◀───────────────────── resposta (texto) ─ │
```

### 2. Anatomia de uma requisição e de uma resposta

Uma requisição HTTP/1.1 tem exatamente esta forma, e nada mais:

```
GET /todos/1 HTTP/1.1⏎               ← linha de requisição: MÉTODO  CAMINHO  VERSÃO
Host: jsonplaceholder.typicode.com⏎  ← cabeçalhos, um por linha, "Nome: valor"
Accept: application/json⏎
⏎                                    ← LINHA VAZIA: acabaram os cabeçalhos
                                     ← corpo (vazio num GET)
```

A resposta é igual, trocando a linha de requisição pela **linha de status**:

```
HTTP/1.1 200 OK⏎                     ← VERSÃO  CÓDIGO  TEXTO
content-type: application/json; charset=utf-8⏎
content-length: 83⏎
⏎
{"userId":1,"id":1,"title":"delectus aut autem","completed":false}
└──────────────────────────── corpo ─────────────────────────────┘
```

O `⏎` é a sequência de dois bytes `\r\n` (CR + LF, `0x0D 0x0A`); a **linha vazia** é o separador obrigatório entre
cabeçalhos e corpo. Este é o texto **real** deste projeto, copiado de
`curl -D - https://jsonplaceholder.typicode.com/todos/1`.

> Quer ver esse texto sendo montado à mão, byte a byte, sobre um socket cru? O subprojeto vizinho faz
> exatamente isso: [`../network/README.md`](../network/README.md), demo 5 (*HTTP raw*). A libcurl não faz nada
> além de automatizar a escrita e a leitura desse texto — mais TLS, redirecionamentos, *chunked encoding* e
> uns duzentos casos de borda.

### 3. Dissecando a URL

```
https :// jsonplaceholder.typicode.com / todos ? userId=1
  │            │                           │         └─ query string: pares chave=valor,
  │            │                           │            separados por &, depois do "?"
  │            │                           └─ caminho (path): qual recurso
  │            └─ host: qual máquina (resolvido por DNS para um IP)
  └─ esquema: http (porta 80, texto puro) ou https (porta 443, criptografado)
```

A **query string** é a forma padrão de mandar parâmetros num GET; o demo 2 usa `?userId=1` para pedir só as tarefas do usuário 1.

### 4. Métodos e códigos de status

O **método** é a primeira palavra da requisição — o verbo. Este projeto usa só `GET`, que é **seguro** (não muda nada
no servidor) e **idempotente** (repetir dez vezes tem o mesmo efeito de uma). Os outros: `POST` cria, `PUT` substitui,
`PATCH` altera em parte, `DELETE` apaga. Já o **código de status** é o número na linha de status, e a primeira casa
dele diz quase tudo:

| Faixa | Significado | Exemplos |
|---|---|---|
| `1xx` | informativo, continue | `100 Continue` |
| `2xx` | **deu certo** | `200 OK`, `201 Created`, `204 No Content` |
| `3xx` | **redirecionamento**: o recurso está noutro lugar, indicado no cabeçalho `Location` | `301 Moved Permanently`, `302 Found` |
| `4xx` | **erro do cliente**: você errou o pedido | `400 Bad Request`, `401 Unauthorized`, `404 Not Found`, `429 Too Many Requests` |
| `5xx` | **erro do servidor**: ele errou | `500 Internal Server Error`, `503 Service Unavailable` |

Guarde esta frase, porque ela é a origem de metade das confusões: **um `404` é uma resposta bem-sucedida do ponto de
vista da rede.** O servidor foi encontrado, respondeu, o texto chegou inteiro; o que deu errado foi o
*pedido*, não o *transporte* — por isso o demo 4 não lança exceção nenhuma. Já um `3xx` se resolve repetindo a
requisição na URL do `Location`, e a libcurl faz isso sozinha com `CURLOPT_FOLLOWLOCATION`.

### 5. Cabeçalhos (*headers*)

Linhas `Nome: valor` que carregam metadados. Os que aparecem neste projeto:

| Cabeçalho | Em quem | Para quê |
|---|---|---|
| `Host` | requisição | qual site, já que um IP pode servir vários (a libcurl preenche sozinha) |
| `Accept` | requisição | que formato eu aceito de volta — aqui, sempre `application/json` |
| `X-Custom-Header` | requisição | cabeçalho inventado por nós; `X-` era a convenção para nomes não padronizados |
| `content-type` | resposta | que formato veio: `application/json; charset=utf-8` |
| `content-length` | resposta | quantos bytes tem o corpo |
| `location` | resposta 3xx | para onde ir |

> ⚠ **Nomes de cabeçalho não distinguem maiúsculas de minúsculas** (RFC 9110 §5.1): `Content-Type` e
> `content-type` são o mesmo cabeçalho. O HTTP/1.1 costuma mandar `Content-Type`; o HTTP/2 obriga tudo
> minúsculo. Por isso `http_client.cpp` normaliza **toda** chave para minúsculas antes de guardá-la — sem
> isso, procurar por `"content-type"` funcionaria ou não conforme a versão do protocolo negociada.

### 6. HTTPS, TLS e "validar o certificado"

`https` é o mesmo HTTP dentro de um túnel criptografado chamado **TLS**. Ao conectar, o servidor apresenta um
**certificado**: um arquivo assinado que diz "eu sou `jsonplaceholder.typicode.com`", assinado por uma
**autoridade certificadora** em que o seu sistema já confia. **Validar o certificado** é conferir duas coisas
independentes: (1) a assinatura fecha com alguma autoridade da lista de confiança do sistema
(`CURLOPT_SSL_VERIFYPEER`); (2) o nome dentro do certificado é o mesmo host da URL (`CURLOPT_SSL_VERIFYHOST`). Sem (1)
qualquer um pode se passar pelo servidor; sem (2), um invasor com certificado válido de *outro* domínio consegue.
Desligar essas opções "porque dava erro" é o atalho que transforma HTTPS em HTTP com senso falso de segurança.

### 7. REST, recurso, coleção, *endpoint*

A API deste projeto segue o estilo **REST**: a URL nomeia um **recurso**, o método diz o que fazer com ele. Cada uma
dessas URLs é um ***endpoint***, um ponto de entrada da API:

```
/todos          → a COLEÇÃO de tarefas    (GET devolve um array JSON)
/todos/1        → o RECURSO de id 1       (GET devolve um objeto JSON)
/todos?userId=1 → a coleção FILTRADA      (GET devolve um array JSON)
```

### 8. O que é JSON

**JSON** (*JavaScript Object Notation*) é um formato de texto com **seis** tipos, e só seis:

| Tipo JSON | Escrita | Tipo C++ correspondente |
|---|---|---|
| objeto | `{"chave": valor, ...}` | `struct`, `std::map<std::string, …>` |
| array | `[valor, valor, ...]` | `std::vector<…>` |
| string | `"texto"` (sempre aspas duplas) | `std::string` |
| número | `42`, `-3.5`, `1e9` (não distingue int de float) | `int`, `double` |
| booleano | `true` / `false` | `bool` |
| nulo | `null` | — |

Um dos objetos que este projeto baixa, na íntegra: `{ "userId": 1, "id": 1, "title": "delectus aut autem",
"completed": false }`

> **A frase-chave:** JSON **não tem tipos definidos pelo usuário**. Não existe "um `Todo`" no arquivo — existe
> um objeto com quatro campos. Alguém precisa escrever a ponte entre esse objeto genérico e a sua
> `struct Todo`. A macro `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` é essa ponte, e a seção "Sem a macro" adiante
> mostra que ela não é mágica.

### 9. libcurl e o modelo do *easy handle*

A **libcurl** é uma biblioteca **em C** que fala HTTP, HTTPS, FTP e mais uma dúzia de protocolos. Das suas três
interfaces usamos a mais simples, a **easy**, que é **síncrona**: a chamada só volta quando a transferência terminou.
O modelo tem exatamente quatro passos:

```c
CURL* h = curl_easy_init();               // 1. cria um "handle": um saco opaco de opções
curl_easy_setopt(h, CURLOPT_URL, "...");  // 2. configura, uma opção por chamada
curl_easy_perform(h);                     // 3. executa; só volta no fim
curl_easy_cleanup(h);                     // 4. destrói o handle e libera tudo
```

O **handle** (`CURL*`) é um ponteiro para um tipo incompleto — você nunca vê o que tem dentro. Ele guarda todas as
opções acumuladas e é reutilizável: dois `perform` no mesmo handle reaproveitam a conexão TCP e o aperto de mão TLS.

### 10. `curl_easy_setopt` é variádica — e isso morde

A assinatura real é `CURLcode curl_easy_setopt(CURL* handle, CURLoption option, ...)`. Aquele `...` significa:
**o compilador não sabe, e não confere, o tipo do terceiro argumento**. Cada `CURLOPT_*` exige um tipo
específico (`long`, `const char*`, ponteiro de função, `void*`) e passar o tipo errado compila sem um aviso sequer,
quebrando em tempo de execução. Daí duas regras que valem sempre: **escreva `1L`, não `1`** (as opções booleanas
esperam um `long`; um `int` literal `1` tem 4 bytes onde a libcurl vai ler 8 — por isso todo o código deste projeto
usa `1L`, `5L`, `30L`); e **nunca passe uma lambda**, pelo motivo do item seguinte.

### 11. O padrão C de *callback* + `void* userdata`

A libcurl não conhece `std::string`. Quando os bytes do corpo chegam, ela precisa entregá-los a alguém — então chama
uma função **que você registrou**, com esta assinatura fixa:

```cpp
size_t nome(char* ptr, size_t size, size_t nmemb, void* userdata);
//          └─ pedaço recebido      └─ size*nmemb  └─ ponteiro que VOCÊ mandou guardar
//             (NÃO termina em '\0') = bytes       (a libcurl só repassa, nunca olha)
```

O truque, universal em bibliotecas C, é o **`void* userdata`**: você registra o endereço do seu objeto com
`CURLOPT_WRITEDATA` e, dentro da função, faz o `static_cast` de volta para o tipo real. É assim que uma
biblioteca C escrita nos anos 90 preenche um `std::string` de 2011. O **retorno** também importa: a função devolve
quantos bytes consumiu, e devolver número diferente de `size * nmemb` faz a libcurl abortar a transferência — é o
mecanismo oficial de "pare, deu problema aqui".

> ⚠ **A armadilha que este projeto evita.** É tentador escrever a *callback* como lambda inline. Só que a
> conversão implícita de lambda-sem-captura para ponteiro de função **não acontece em argumento variádico**:
> nada, dentro do `...`, dispara essa conversão. O objeto-fecho é passado como está e a libcurl o lê como se
> fosse endereço de função — comportamento indefinido. E isto compila **limpo**, com `-Wall -Wextra`:
>
> ```cpp
> // ⚠ NÃO FAÇA — compila sem aviso, quebra em execução
> curl_easy_setopt(h, CURLOPT_WRITEFUNCTION,
>     [](char* p, size_t s, size_t n, std::string* b) { b->append(p, s*n); return s*n; });
> ```
>
> Repare no segundo erro escondido no mesmo trecho: o quarto parâmetro está tipado como `std::string*`, mas a
> libcurl exige `void*`. Uma função `static` livre com `void*` no quarto parâmetro resolve os dois problemas
> de uma vez — é o que `http_client.cpp` faz.

### 12. RAII, em um parágrafo

**RAII** (*Resource Acquisition Is Initialization*) é a regra que organiza todo recurso em C++: **adquira no
construtor, libere no destrutor**. O ganho não é digitar menos — é que o destrutor de um objeto local roda
**em toda saída do escopo**, inclusive quando uma exceção está subindo a pilha. É por isso que os invólucros
`CurlHandle` e `CurlSlist` existem aqui; fundamentos em
[`../smart-pointers/README.md`](../smart-pointers/README.md).

---

## Por que estudar isto?

O problema: **quase todo programa útil precisa falar com um servidor**, e C++ não traz nada disso na biblioteca
padrão. Não existe `std::http`. Sobram três caminhos, e dois são ruins.

| Caminho | O que acontece |
|---|---|
| `system("curl ... > /tmp/x")` | cria um processo por requisição; passa dados por arquivo temporário; a URL vira injeção de shell se vier do usuário; o erro só chega como código de saída; nada de *timeout* |
| libcurl **crua**, à moda C | funciona, mas cada função vira 30 linhas de `setopt` repetidas — e todo `throw` no meio vaza o handle e a lista de cabeçalhos |
| **um invólucro C++ fino** (este projeto) | `auto r = http::get(url);` — uma linha. O RAII garante a liberação mesmo com exceção; o erro de rede vira exceção, o erro de HTTP vira dado |

A lição que se leva daqui é maior que HTTP: **como embrulhar uma API C em C++ sem perder segurança nem desempenho**. O
padrão handle-opaco + `init`/`cleanup` + *callback* com `void* userdata` é o mesmo do SQLite, do OpenSSL e do FFmpeg.

---

## Estrutura de arquivos

```
http-client/
├── README.md          ← este arquivo
├── http_client.hpp    ← a interface: Headers, Response, HttpError, get()   (26 linhas)
├── http_client.cpp    ← implementação: RAII, callbacks, opções da libcurl (120 linhas)
├── main.cpp           ← os 4 demos + struct Todo + impressão colorida     (133 linhas)
└── meson.build        ← alvo executable('http_client'), deps fmt/json/curl
```

---

## O exemplo mínimo

Um programa completo que usa a biblioteca deste projeto — o menor que compila e faz sentido. Nenhum `new`, nenhum
`delete`, nenhum `free`; o JSON só entra depois:

```cpp
#include "http_client.hpp"
#include <fmt/core.h>

int main() {
    try {
        auto r = http::get("https://jsonplaceholder.typicode.com/todos/1");
        fmt::print("status : {}\n", r.status_code);        // 200
        fmt::print("bytes  : {}\n", r.body.size());        // 83
        fmt::print("corpo  : {}\n", r.body.substr(0, 60)); // {"userId": 1, ...
    } catch (const http::HttpError& e) {
        fmt::print(stderr, "falha de rede: {}\n", e.what());
        return 1;
    }
}
```

---

## A interface real

Este é o conteúdo de `http_client.hpp`, sem cortes:

```cpp
#pragma once
#include <string>
#include <map>
#include <stdexcept>

namespace http {

using Headers = std::map<std::string, std::string>;

struct Response {
    long        status_code;
    std::string body;
    Headers     headers;
};

struct HttpError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// GET síncrono. Lança HttpError em caso de falha de transporte.
// Erros HTTP (4xx, 5xx) são retornados normalmente no status_code.
Response get(const std::string& url, const Headers& request_headers = {});

} // namespace http
```

Quatro decisões de projeto, todas visíveis acima. **`long status_code`, não `int`**, porque é o que
`curl_easy_getinfo` devolve (`CURLINFO_RESPONSE_CODE` escreve num `long`) — converter na fronteira só
acrescentaria um lugar para errar. **`std::map`, não `std::unordered_map`**, porque são ~20 cabeçalhos: a árvore
ordenada é mais simples, mantém a saída em ordem alfabética e evita o custo de *hash*. **Um único
`get`, com argumento padrão `= {}`**, porque duas sobrecargas dariam o mesmo com o dobro de código. E
**`struct HttpError : std::runtime_error`**, herdando os construtores com
`using std::runtime_error::runtime_error;` — `struct` porque tudo é público mesmo, e herdar de
`std::runtime_error` faz `catch (const std::exception&)` também pegá-la (mais em
[`../errors/README.md`](../errors/README.md)).

---

## A implementação, por partes

### Os dois invólucros RAII

```cpp
struct CurlHandle {
    CURL* h;
    CurlHandle() : h(curl_easy_init()) { if (!h) throw HttpError("curl_easy_init failed"); }
    ~CurlHandle() { curl_easy_cleanup(h); }

    CurlHandle(const CurlHandle&)            = delete;   // não copiável
    CurlHandle& operator=(const CurlHandle&) = delete;
};

struct CurlSlist {
    curl_slist* list = nullptr;
    void append(const std::string& h) { list = curl_slist_append(list, h.c_str()); }
    ~CurlSlist() { if (list) curl_slist_free_all(list); }
};
```

#### ⚠ Por que CurlHandle e CurlSlist existem

Compare. **Antes**, na versão "natural", em C:

```cpp
CURL* curl = curl_easy_init();
curl_slist* slist = nullptr;
/* ... 15 setopt ... */
CURLcode res = curl_easy_perform(curl);
if (res != CURLE_OK)
    throw HttpError(curl_easy_strerror(res));   // ⚠ o throw sai daqui...
curl_slist_free_all(slist);                     // ⚠ ...e estas duas linhas
curl_easy_cleanup(curl);                        // ⚠    NUNCA rodam
```

O `throw` desvia o fluxo direto para o `catch` do chamador; as duas liberações ficam para trás e **vazam** o handle
(que contém sockets, buffers e o contexto TLS) e a lista de cabeçalhos — uma vez por requisição que falhar, e num laço
de *retry*, a cada tentativa. **Depois**, com os invólucros, `CurlHandle curl;` é uma variável **local**: quando o
`throw` desenrola a pilha, `~CurlHandle` roda, e a linguagem garante isso. Não há caminho de saída que escape — nem
`return`, nem `throw`, nem um `return` novo que alguém acrescente daqui a dois anos no meio da função. O `= delete`
nas cópias é a outra metade da regra: copiar um `CurlHandle` daria dois objetos com o **mesmo** `CURL*`, e o segundo
destrutor faria `curl_easy_cleanup` num ponteiro já liberado (*double free*) — proibir a cópia torna o erro impossível
de escrever.

### As duas *callbacks*

```cpp
// Acumula chunks do body em uma std::string
static size_t write_body(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);   // ← o void* volta a ser string*
    buf->append(ptr, size * nmemb);                    // ← append com TAMANHO: não há '\0'
    return size * nmemb;                               // ← "consumi tudo"
}
```

`static` porque a função só interessa a esta unidade de tradução — sem isso o símbolo ficaria visível ao
ligador e poderia colidir com outro `write_body` (ver [`../linkage/README.md`](../linkage/README.md)). Já
`write_header` recebe **uma linha por chamada**, no formato `Nome: valor\r\n`, e faz três coisas: corta no
primeiro `:`, apara espaços e `\r\n` das duas metades, e converte a chave para minúsculas antes de guardá-la no
`Headers`. A linha de status (`HTTP/1.1 200 OK`) também passa por aqui, mas não tem `:` — cai fora no `if` e é
ignorada, que é exatamente o desejado.

### As opções, uma tabela

Tudo o que `http::get` configura antes de executar:

| Opção | Valor | O que faz |
|---|---|---|
| `CURLOPT_URL` | `url.c_str()` | o destino |
| `CURLOPT_FOLLOWLOCATION` | `1L` | segue `301`/`302` refazendo a requisição no `Location` |
| `CURLOPT_MAXREDIRS` | `5L` | teto de saltos; sem ele um servidor mal configurado prende o programa num ciclo |
| `CURLOPT_TIMEOUT` | `30L` | tempo **total** da operação, em segundos |
| `CURLOPT_CONNECTTIMEOUT` | `10L` | tempo só da fase de conexão (DNS + TCP + TLS) |
| `CURLOPT_SSL_VERIFYPEER` | `1L` | exige certificado assinado por autoridade confiável |
| `CURLOPT_SSL_VERIFYHOST` | `2L` | exige que o nome no certificado bata com o host |
| `CURLOPT_WRITEFUNCTION` / `CURLOPT_WRITEDATA` | `write_body` / `&body` | a *callback* do corpo e o `void*` que chega nela |
| `CURLOPT_HEADERFUNCTION` / `CURLOPT_HEADERDATA` | `write_header` / `&resp_headers` | a *callback* dos cabeçalhos e o `void*` dela |
| `CURLOPT_HTTPHEADER` | `slist.list` | os cabeçalhos que **enviamos** |

> **Os dois *timeouts* não são redundantes.** `CONNECTTIMEOUT` protege do host que não responde ao aperto de
> mão; `TIMEOUT` protege do servidor que aceita a conexão e depois envia um byte por minuto. A libcurl **não**
> tem limite padrão para a transferência total. E `SSL_VERIFYHOST` só admite `0` ou `2`: o `1` era um modo de
> depuração removido há muito, e versões atuais o rejeitam com `CURLE_BAD_FUNCTION_ARGUMENT`.

### O `Accept` que vai sempre junto

```cpp
CurlSlist slist;
slist.append("Accept: application/json");     // ← embutido, em TODA requisição
for (auto& [k, v] : request_headers)
    slist.append(k + ": " + v);
curl_easy_setopt(curl.h, CURLOPT_HTTPHEADER, slist.list);
```

Decisão de projeto que o chamador precisa conhecer: **`http::get` sempre envia `Accept: application/json`**, mesmo que
você não peça nada. É coerente com o propósito da biblioteca, mas significa que passar
`{{"Accept", "text/html"}}` não *substitui* o cabeçalho — acrescenta um segundo, e o resultado passa a
depender do servidor. Para outro formato, mude a linha do `append`.

---

## JSON com nlohmann

### O modelo de domínio

```cpp
struct Todo {
    int         id;
    int         userId;
    std::string title;
    bool        completed;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Todo, id, userId, title, completed)
```

*Non-intrusive* quer dizer: a macro fica **fora** da `struct`, e a `struct` continua um agregado limpo, sem
herança nem membros extras — o que permite usar a técnica em tipos de terceiros que você não pode alterar.

### Sem a macro: o que ela gera

A macro não é mágica. Ela expande, em essência, para estas duas funções livres — escrevê-las à mão dá exatamente o
mesmo resultado:

```cpp
void from_json(const nlohmann::json& j, Todo& t) {   // JSON → C++ : chamada por j.get<Todo>()
    j.at("id").get_to(t.id);          j.at("userId").get_to(t.userId);
    j.at("title").get_to(t.title);    j.at("completed").get_to(t.completed);
}

void to_json(nlohmann::json& j, const Todo& t) {     // C++ → JSON : chamada por json j = todo;
    j = nlohmann::json{{"id", t.id}, {"userId", t.userId},
                       {"title", t.title}, {"completed", t.completed}};
}
```

A biblioteca as encontra por **ADL** (busca dependente de argumento): como `Todo` mora no namespace global,
`from_json`/`to_json` no namespace global são achadas automaticamente; se `Todo` estivesse em `namespace app`,
as funções teriam de estar em `namespace app` também.

> ⚠ A versão gerada usa `at()`, que **lança** `nlohmann::json::out_of_range` se o campo não existir — e isso é
> uma virtude: campo faltando vira erro alto e claro, não um zero silencioso. Para campos opcionais existe
> `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`, que preenche com o valor do objeto recém-construído.

### Uso

```cpp
Todo t        = nlohmann::json::parse(resp.body).get<Todo>();               // objeto → struct
auto todos    = nlohmann::json::parse(resp.body).get<std::vector<Todo>>();  // array  → vector
auto post     = nlohmann::json::parse(resp.body);                           // acesso dinâmico
auto titulo   = post["title"].get<std::string>();
int  curtidas = post.value("likes", 0);   // 0 se a chave não existir
```

> ⚠ `post["chave"]` num objeto **não-const** *cria* a chave com valor `null` se ela não existir;
> `post.at("chave")` lança; `post.value("chave", padrao)` devolve o padrão. `[]` é o único dos três que
> altera o JSON só por lê-lo.

---

## Os demos, um a um

Os quatro rodam em sequência ao executar `./dist/bin/http_client`, cada um dentro do seu próprio
`try/catch (const http::HttpError&)`.

### Demo 1 — GET de um recurso único

O problema mais simples: baixar **um** objeto e transformá-lo numa `struct`.

```cpp
auto resp = http::get("https://jsonplaceholder.typicode.com/todos/1");

fmt::print("status       : {}\n", resp.status_code);                 // 200
fmt::print("content-type : {}\n", resp.headers.count("content-type") // application/json; charset=utf-8
                                ? resp.headers.at("content-type") : "(ausente)");

Todo todo = nlohmann::json::parse(resp.body).get<Todo>();
print_todo(todo);                                                    //   [  1] ○ delectus aut autem
```

> ⚠ **Por que `count()` antes de `at()`.** `std::map::at` lança `std::out_of_range` quando a chave não existe,
> e **nada obriga um servidor a mandar `Content-Type`** (uma resposta `204 No Content` normalmente não manda).
> Escrever `resp.headers.at("content-type")` direto funciona em teste e derruba o programa em produção, contra
> o primeiro servidor que se comportar diferente. O `count()` — ou `find()`, ou `contains()` do C++20 — é o
> que torna o código honesto.

### Demo 2 — GET de uma coleção, com filtro em memória

Agora o corpo é um **array** JSON, e o mesmo `get<T>()` preenche um `std::vector`:

```cpp
auto resp  = http::get("https://jsonplaceholder.typicode.com/todos?userId=1");
auto todos = nlohmann::json::parse(resp.body).get<std::vector<Todo>>();

std::vector<Todo> done;
for (auto& t : todos)
    if (t.completed) done.push_back(t);
// medido: Total = 20, Concluídas = 11
```

> **Filtrar aqui ou no servidor?** Esta API aceitaria `?userId=1&completed=true` e devolveria só as 11.
> Filtrar **no servidor** economiza banda e é o certo quando a coleção é grande; filtrar **em memória** faz
> sentido quando você já precisa de tudo por outro motivo. O demo filtra localmente de propósito, para mostrar
> que `vector<Todo>` é um `vector` comum — nada de "objetos JSON" espalhados pelo código depois do `parse`.

### Demo 3 — Enviando um cabeçalho customizado

```cpp
auto resp = http::get("https://jsonplaceholder.typicode.com/posts/1",
                      {{"X-Custom-Header", "cpp_academy"}});          // status medido: 200

auto post = nlohmann::json::parse(resp.body);
fmt::print("título: {}\n", post["title"].get<std::string>());
// título: sunt aut facere repellat provident occaecati excepturi optio reprehenderit
```

O `Headers` é um `std::map`, então a chamada aceita inicialização por chaves. **`Accept` não aparece aqui de
propósito**: `http::get` já o envia sozinho. O único cabeçalho acrescentado é o `X-Custom-Header` — e, sendo um
cabeçalho de **requisição**, ele não volta na resposta; `jsonplaceholder` simplesmente o ignora. Para *ver* o que você
mandou, aponte a mesma chamada para `https://httpbin.org/headers`, que ecoa no corpo JSON os cabeçalhos recebidos.

### Demo 4 — Um 404 não é uma exceção

```cpp
auto resp = http::get("https://jsonplaceholder.typicode.com/todos/9999");
fmt::print("status: {}\n", resp.status_code);   // 404
fmt::print("body  : {}\n", resp.body);          // {}
```

Nenhum `catch` dispara. A conexão funcionou, o TLS validou, o corpo chegou inteiro — só que o recurso não existe, e o
servidor disse isso pelo canal correto: o código de status. O corpo é um objeto JSON vazio, `{}`, que ainda assim é
JSON válido.

---

## Erros: três camadas independentes

Esta é a distinção que separa código de rede robusto de código otimista. **Cada camada falha por um motivo diferente e
se detecta de um jeito diferente.**

| Camada | Exemplo de falha | Como se manifesta | O que fazer |
|---|---|---|---|
| **1. Transporte** | DNS não resolve, conexão recusada, certificado inválido, *timeout* de 30 s | `throw http::HttpError` com a mensagem de `curl_easy_strerror` | `try/catch`; vale tentar de novo com espera |
| **2. Protocolo HTTP** | `404`, `401`, `429`, `500` | **valor** em `resp.status_code`; nenhuma exceção | testar a faixa; só `5xx` e `429` merecem *retry* |
| **3. Conteúdo** | corpo não é JSON; falta o campo `title`; `id` veio como string | `nlohmann::json::parse_error` ou `::out_of_range` no `parse`/`get<T>` | `try/catch` em volta do `parse`; registrar o corpo cru |

```cpp
try {
    auto r = http::get(url);
    if (r.status_code / 100 != 2) {                                       // camada 2
        fmt::print(stderr, "HTTP {} — corpo: {}\n", r.status_code, r.body);
        return;
    }
    auto todos = nlohmann::json::parse(r.body).get<std::vector<Todo>>();  // camada 3

} catch (const http::HttpError& e) {                                      // camada 1
    fmt::print(stderr, "rede: {}\n", e.what());
} catch (const nlohmann::json::exception& e) {                            // camada 3
    fmt::print(stderr, "json: {}\n", e.what());
}
```

Sobre **repetir a requisição**: só faz sentido para `5xx`, `429` e falhas de transporte, e apenas porque `GET` é
idempotente — repetir um `POST` pode criar dois pedidos. A prática padrão é *exponential backoff* (1 s, 2 s, 4 s, com
teto de tentativas); um `429` costuma trazer o cabeçalho `retry-after` dizendo quantos segundos esperar, e esta API
manda `x-ratelimit-remaining` em toda resposta.

---

## ⚠ Duas armadilhas de C++ que este demo pisou

Estão documentadas em `main.cpp`, na função `print_separator`, e não têm nada a ver com HTTP — são erros de
**linguagem**, e os dois eram silenciosos.

### (a) `'─'` entre aspas simples

O caractere `─` é U+2500, que em UTF-8 ocupa **três bytes**. Entre **aspas simples** ele não vira um `char`: vira uma
*multi-character constant*, um `int` de valor definido pela implementação, truncado na conversão. O GCC avisa com
`-Wmultichar` e `-Woverflow` (*"changes value from 14849152 to -128"*), mas o código compila e imprime bytes
inválidos.

```cpp
std::string linha(largura, '─');    // ⚠ ERRADO: um char guarda UM byte

std::string linha;                  // ✓ certo
linha.reserve(largura * 3);         //   3 bytes por '─' em UTF-8
for (std::size_t i = 0; i < largura; ++i) linha += "─";   // string, não char
```

A regra geral: **`char` é um byte, não um caractere**; fora do ASCII, use `std::string` (ou `char8_t`/`char32_t`) — ver [`../strings/README.md`](../strings/README.md).

### (b) `40 - title.size()` com `size_t`

`std::string::size()` devolve `std::size_t`, que é **sem sinal**. Com um título de 41 caracteres, `40 - 41`
não dá `-1`: dá **18 446 744 073 709 551 615**, e o `std::string` seguinte tentaria alocar 16 exabytes.

```cpp
const std::size_t largura = title.size() < 40 ? 40 - title.size() : 0;   // ✓
```

Subtração entre inteiros sem sinal **nunca é negativa** — ela envolve para o topo da faixa. Ou se testa antes, como
acima, ou se converte para um tipo com sinal antes de subtrair. A mecânica de promoção e conversão está em
[`../bits/README.md`](../bits/README.md) (`demo_operadores.hpp`).

---

## Como validar

```bash
make build && ./dist/bin/http_client ; echo "saída = $?"
```

Espere **quatro** blocos separados por uma barra em negrito, nesta ordem, e código de saída `0`:

```
── GET /todos/1 ────────────────────────
status       : 200
content-type : application/json; charset=utf-8
  [  1] ○ delectus aut autem

── GET /todos?userId=1 ─────────────────
status: 200
Total    : 20
Concluídas: 11
  [  4] ✓ et porro tempora      ← e mais 10 linhas verdes

── GET /posts/1 com header ─────────────
status: 200
título: sunt aut facere repellat provident occaecati excepturi optio reprehenderit

── GET /todos/9999 (404) ───────────────
status: 404
body  : {}
```

Se algo divergir, compare com o `curl` de linha de comando — e confirme o nome do alvo:

```bash
curl -s https://jsonplaceholder.typicode.com/todos/1                     # mesmo JSON do demo 1
curl -s -D - -o /dev/null https://jsonplaceholder.typicode.com/todos/1   # os cabeçalhos crus
grep -n "executable(" src/http-client/meson.build                        # deve dizer http_client
```

| Sintoma | Causa provável |
|---|---|
| `erro de transporte: Could not resolve host` | sem internet ou sem DNS |
| `erro de transporte: SSL peer certificate ... not OK` | proxy corporativo interceptando TLS, ou certificados do sistema desatualizados |
| `erro de transporte: Timeout was reached` | rede muito lenta; os limites são 10 s para conectar e 30 s no total |
| `[json.exception.parse_error...]` | o corpo não era JSON — imprima `resp.body` para ver o que veio |

**Exercícios.** (1) Compare `resp.headers.at("content-length")` com `resp.body.size()` no demo 1. (2) Acrescente
`http::post(url, body, headers)` com `CURLOPT_POSTFIELDS`. (3) Meça 10 chamadas a `http::get` (10 handles)
contra 10 `curl_easy_perform` no **mesmo** handle — a diferença é o aperto de mão TLS evitado.

---

## Referências

**Neste repositório** — [`../network/README.md`](../network/README.md): sockets, Asio e o **HTTP escrito à
mão** sobre TCP cru (demo 5), a camada de baixo do que a libcurl automatiza ·
[`../errors/README.md`](../errors/README.md): hierarquia de exceções, `std::error_code`, `std::expected` ·
[`../smart-pointers/README.md`](../smart-pointers/README.md): RAII e *deleters* customizados, a forma genérica do que
`CurlHandle` faz à mão · [`../database/README.md`](../database/README.md): o mesmo invólucro RAII aplicado à API C do
SQLite · [`../strings/README.md`](../strings/README.md), [`../bits/README.md`](../bits/README.md) e
[`../linkage/README.md`](../linkage/README.md): as armadilhas da seção anterior.

**Fora do repositório**

- libcurl, interface *easy*: <https://curl.se/libcurl/c/> — opções em <https://curl.se/libcurl/c/curl_easy_setopt.html>
- nlohmann/json: <https://json.nlohmann.me/> (em especial *Arbitrary Type Conversions*)
- RFC 9110 — HTTP Semantics: <https://www.rfc-editor.org/rfc/rfc9110.html>
- RFC 8259 — JSON: <https://www.rfc-editor.org/rfc/rfc8259.html>
- JSONPlaceholder, a API de testes usada aqui: <https://jsonplaceholder.typicode.com/>
