# http-client — Cliente HTTP com JSON usando libcurl + nlohmann/json

## O que é este projeto?

Demonstra como fazer **requisições HTTP/HTTPS** em C++ e processar respostas **JSON** usando duas bibliotecas populares: `libcurl` para o transporte de rede e `nlohmann/json` para serialização/deserialização.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| `libcurl` wrapper | Encapsula curl em interface C++ simples e segura |
| `nlohmann/json` | Parser/serializer JSON com mapeamento automático de structs |
| `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` | Macro para serialização sem modificar a struct |
| Exceções customizadas | `http::HttpError` para erros de transporte |
| Headers customizados | Enviar `X-Custom-Header` e `Accept` customizados |

---

## Estrutura de arquivos

```
http-client/
├── main.cpp          ← 4 demos: GET, lista+filtro, header customizado, 404
├── http_client.hpp   ← declaração de http::get, HttpResponse, HttpError
├── http_client.cpp   ← implementação via libcurl
└── meson.build
```

---

## A interface `http::get`

```cpp
// http_client.hpp
namespace http {

struct HttpResponse {
    int                                status_code;
    std::string                        body;
    std::unordered_map<std::string, std::string> headers;
};

class HttpError : public std::runtime_error {
    using runtime_error::runtime_error;
};

// GET simples
HttpResponse get(const std::string& url);

// GET com headers customizados
HttpResponse get(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers
);

} // namespace http
```

---

## `nlohmann/json` — Serialização automática de structs

### Definindo o modelo

```cpp
struct Todo {
    int         id;
    int         userId;
    std::string title;
    bool        completed;
};

// Esta macro gera automaticamente:
// - from_json(const json& j, Todo& t) — desserialização
// - to_json(json& j, const Todo& t)   — serialização
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Todo, id, userId, title, completed)
```

### Deserialização

```cpp
// JSON → struct
std::string json_str = R"({"id":1,"userId":1,"title":"foo","completed":false})";
Todo todo = json::parse(json_str).get<Todo>();
fmt::print("{}: {}\n", todo.id, todo.title);

// JSON array → vector
std::string array_str = "[{...}, {...}]";
auto todos = json::parse(array_str).get<std::vector<Todo>>();
```

### Serialização

```cpp
// struct → JSON
Todo t{1, 1, "comprar leite", false};
json j = t;
fmt::print("{}\n", j.dump(4)); // indentado com 4 espaços
```

---

## Demo 1: GET único recurso

```cpp
auto resp = http::get("https://jsonplaceholder.typicode.com/todos/1");

fmt::print("status: {}\n", resp.status_code); // 200
fmt::print("tipo: {}\n", resp.headers.at("content-type")); // application/json

Todo todo = json::parse(resp.body).get<Todo>();
// {id: 1, userId: 1, title: "delectus aut autem", completed: false}
```

---

## Demo 2: GET lista com filtro em memória

```cpp
auto resp = http::get("https://jsonplaceholder.typicode.com/todos?userId=1");
auto todos = json::parse(resp.body).get<std::vector<Todo>>();

// Filtro em C++ (poderia ser ?completed=true na query, mas demonstra filtro local)
std::vector<Todo> done;
for (auto& t : todos)
    if (t.completed) done.push_back(t);

fmt::print("Total: {}, Concluídas: {}\n", todos.size(), done.size());
// Total: 20, Concluídas: 11
```

---

## Demo 3: Header customizado

```cpp
auto resp = http::get(
    "https://jsonplaceholder.typicode.com/posts/1",
    {{"X-Custom-Header", "cpp_academy"},
     {"Accept", "application/json"}}
);

auto post = json::parse(resp.body);
fmt::print("título: {}\n", post["title"].get<std::string>());
```

---

## Demo 4: Tratando status 404

```cpp
auto resp = http::get("https://jsonplaceholder.typicode.com/todos/9999");
fmt::print("status: {}\n", resp.status_code); // 404
fmt::print("body: {}\n", resp.body);           // {}
```

`http::get` lança `HttpError` apenas em falhas de **transporte** (sem conexão, SSL inválido, timeout), não para status HTTP 4xx/5xx — você verifica `resp.status_code`.

---

## Implementação interna com libcurl

```cpp
// http_client.cpp
HttpResponse get(const std::string& url,
                 const std::unordered_map<std::string, std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) throw HttpError("curl_easy_init falhou");

    std::string body_buffer;
    std::unordered_map<std::string, std::string> resp_headers;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // segue redirects
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // valida certificado

    // Callback para acumular o body
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        [](char* ptr, size_t size, size_t nmemb, std::string* buf) {
            buf->append(ptr, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_buffer);

    // Adiciona headers customizados
    curl_slist* slist = nullptr;
    for (auto& [k, v] : headers)
        slist = curl_slist_append(slist, (k + ": " + v).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        throw HttpError(curl_easy_strerror(res));

    long status;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);

    return { static_cast<int>(status), body_buffer, resp_headers };
}
```

---

## `nlohmann/json` — Acesso dinâmico

```cpp
json j = json::parse(R"({"debug": true, "max_alunos": 30})");

bool debug = j["debug"].get<bool>();      // true
int max    = j.value("max_alunos", 0);    // 30 (com default)

// Verificação de tipo
j["debug"].is_boolean();  // true
j["debug"].is_number();   // false

// Iteração
for (auto& [key, val] : j.items()) {
    fmt::print("{}: {}\n", key, val.dump());
}
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja http_client
./bin/http_client
```

**Requer conexão com a internet** para acessar `jsonplaceholder.typicode.com`.

---

## Dependências

- `libcurl` — biblioteca de transferência HTTP/HTTPS
- `nlohmann/json` — parser JSON header-only
- `fmt` — formatação de saída
