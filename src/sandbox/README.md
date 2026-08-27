# sandbox — JSON em C++ com `nlohmann/json`

## O que é este projeto?

Um executável com **sete demos numeradas** que ensinam serialização JSON em C++ do zero
usando a biblioteca [`nlohmann/json`](https://github.com/nlohmann/json) — construir, ler de
texto, converter `struct` ↔ JSON, ler e gravar arquivo, tratar os erros, evitar a armadilha
do `operator[]` e comparar JSON com os formatos binários.

Ele é também o **rascunho do repositório**: se você quiser testar uma ideia rápida de C++,
apague o corpo do `main()` e escreva a sua. É o único alvo do projeto que existe justamente
para ser sujado.

```bash
make build                       # compila tudo (build/ é o diretório de build)
./build/src/sandbox/sandbox      # roda direto da árvore de build

make install                     # copia os binários para dist/
./dist/bin/sandbox               # o mesmo binário, instalado
```

> ⚠ O alvo do Meson chama-se **`sandbox`**, e não `main`. Ele já se chamou `main`, e isso
> colidia com o `main` de `src/collision/`: os dois instalavam em `dist/bin/main` e um
> sobrescrevia o outro. Confira você mesmo:
> `grep -n "executable(" src/sandbox/meson.build`

---

## Antes de começar: os sete conceitos

Se algum destes for novo, leia esta seção inteira antes de rodar qualquer coisa. Nada aqui
supõe mais do que C++ básico.

### 1. O que é JSON

**JSON** (*JavaScript Object Notation*) é um formato de **texto** para representar dados
estruturados. Texto puro: você pode abri-lo no bloco de notas, mandá-lo por HTTP, colá-lo
num e-mail. A gramática inteira cabe numa página, e tem exatamente **seis tipos**:

| Tipo JSON | Como se escreve | O que vira em C++ (`nlohmann`) |
|---|---|---|
| `null` | `null` | `nullptr_t` |
| booleano | `true` / `false` | `bool` |
| número | `42`, `-3.5`, `1e10` | `int64_t`, `uint64_t` ou `double` |
| string | `"texto"` (sempre aspas **duplas**) | `std::string` |
| array | `[1, 2, 3]` — ordenado, tipos podem ser mistos | `std::vector<json>` |
| objeto | `{"chave": valor, ...}` — pares chave→valor | `std::map<std::string, json>` |

Arrays e objetos aninham à vontade. Um documento JSON válido é **um único** valor no topo —
normalmente um objeto:

```json
{
  "curso": "C++ moderno",
  "alunos": [
    {"nome": "Lucas", "idade": 25, "nota": 9.5},
    {"nome": "Ana",   "idade": 22, "nota": 8.0}
  ]
}
```

O que JSON **não** tem, e que costuma pegar quem vem de outra linguagem: nada de comentários,
nada de vírgula sobrando no fim da lista, nada de aspas simples, e nada de distinção entre
inteiro e ponto flutuante (é tudo "número"). A gramática formal está na
[RFC 8259](https://www.rfc-editor.org/rfc/rfc8259).

### 2. O que é serializar e desserializar

```
   struct C++ na memória                 texto JSON
   ┌──────────────────────┐              ┌───────────────────────────┐
   │ Aluno {              │  serializar  │ {"idade":25,              │
   │   nome  = "Lucas"    │ ───────────▶ │  "nome":"Lucas",          │
   │   idade = 25         │              │  "nota":9.5}              │
   │   nota  = 9.5        │ ◀─────────── │                           │
   │ }                    │ desserializar└───────────────────────────┘
   └──────────────────────┘
```

**Serializar** é transformar um objeto que vive na memória do processo numa sequência de
bytes que pode sair dele — para um arquivo, um socket, outro programa. **Desserializar** é o
caminho de volta.

Por que não basta um `memcpy` da struct? Porque os bytes crus dependem de tudo: ordem dos
membros, *padding* de alinhamento, tamanho de `int`, endianness, e no caso de `std::string`
os dados nem estão dentro da struct — há um ponteiro para outro lugar. Um `memcpy` de
`Aluno` gravaria um endereço de memória, inútil no próximo processo. (O subprojeto
[`layout`](../layout/README.md) mede exatamente esses buracos de padding.)

### 3. `nlohmann::json` é um tipo-soma

Esta é a parte que estranha quem só conhece C++ estático. Em C++ uma variável tem **um**
tipo, fixado em compilação. `nlohmann::json` quebra essa expectativa de propósito: **uma
mesma variável pode ser qualquer um dos seis tipos JSON, e mudar em tempo de execução.**

```cpp
json j;                    // agora é null
j = 42;                    // agora é número
j = "texto";               // agora é string
j["chave"] = 1;            // agora é objeto (a atribuição converteu)
std::cout << j.type_name(); // "object"
```

Por dentro é uma união etiquetada (*tagged union*) — a mesma ideia de
[`std::variant`](../vocabulary_types/README.md), só que com uma API que finge ser dinâmica.
Daí vêm os métodos de interrogação: `is_null()`, `is_number()`, `is_string()`,
`is_array()`, `is_object()`, `is_discarded()`.

### 4. `to_json` / `from_json` e o ADL

Como a biblioteca sabe converter o **seu** `struct Aluno`, que ela nunca viu? Você fornece
duas funções livres com nomes exatos:

```cpp
void to_json(json& j, const Aluno& a);    // Aluno → json
void from_json(const json& j, Aluno& a);  // json → Aluno
```

E ela as encontra por **ADL** (*argument-dependent lookup*, "busca dependente de
argumento"): ao compilar `json j = aluno;`, o compilador procura a função `to_json` não só
no escopo atual, mas também **no namespace onde `Aluno` foi declarado**. É o mesmo mecanismo
que faz `std::cout << meu_tipo` achar o seu `operator<<` — veja
[`operators`](../operators/README.md).

Consequência prática: as duas funções precisam morar **no mesmo namespace do seu tipo**.
Escondê-las noutro lugar e depois se perguntar por que "a biblioteca não acha" é o erro
número um de quem usa `nlohmann` pela primeira vez.

### 5. A macro `NLOHMANN_DEFINE_TYPE_*`

Escrever as duas funções à mão é repetitivo, então a biblioteca oferece uma macro que as
gera. Não há mágica nenhuma: ela **expande exatamente para essas duas funções**.

| Macro | Onde se coloca | Para que serve |
|---|---|---|
| `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(T, campos...)` | fora da classe | campos **públicos** |
| `NLOHMANN_DEFINE_TYPE_INTRUSIVE(T, campos...)` | **dentro** da classe | alcança campos **privados** |
| `..._WITH_DEFAULT(T, campos...)` | (as duas variantes existem) | chave ausente → valor padrão em vez de exceção |

O `main.cpp` deste diretório usa as duas abordagens lado a lado — `Aluno` com a macro,
`Curso` com as funções escritas à mão — para você ver que o código de **uso** é idêntico.

### 6. `dump()` e `parse()`

Os dois verbos centrais, e é fácil confundir a direção:

```
   texto  ──json::parse(texto)──▶  json  ──j.dump()──▶  texto
```

`dump()` sem argumento produz a forma compacta, sem espaço nenhum. `dump(n)` indenta com
`n` espaços por nível e quebra linha. `parse()` aceita uma `std::string`, um par de
iteradores, ou **um `std::istream`** — é assim que se lê um arquivo.

### 7. Exceção: o que a biblioteca lança quando algo dá errado

`nlohmann/json` sinaliza erro **lançando exceções**, todas derivadas de
`nlohmann::json::exception`, que carrega um `id` numérico **estável** (use o `id`, não o
texto do `what()`, que muda entre versões). São três famílias, e você vai encontrar as três:

| Classe | `id` visto no demo 5 | Quando |
|---|---|---|
| `json::parse_error` | `101` | o texto não é JSON válido |
| `json::type_error` | `302` | o valor existe, mas não é do tipo pedido |
| `json::out_of_range` | `403` | `.at("chave")` com chave que não existe |

Se você ainda não estudou exceções, garantias e `try`/`catch`, o subprojeto
[`errors`](../errors/README.md) cobre o assunto inteiro.

---

## Por que estudar isto?

O problema é sempre o mesmo: **um programa precisa entregar dados a outro programa (ou a si
mesmo, mais tarde) e os dois não compartilham memória.**

| Sem um formato de serialização | Com JSON |
|---|---|
| configuração compilada dentro do binário — mudar exige recompilar | `config.json` ao lado do executável |
| protocolo próprio: você escreve o parser, e o parser tem bugs | `json::parse` já é testado por milhões de usuários |
| `fwrite` da struct crua — quebra ao mudar um `int` para `long` | o texto continua legível e versionável |
| logs binários que só o seu programa lê | `jq`, `curl`, o navegador e um humano leem |
| falar com uma API REST em C++ é impraticável | é o formato nativo delas |

Onde aparece de verdade num programa C++:

- **Configuração** — ler `config.json` na inicialização (demo 4).
- **API HTTP** — corpo de requisição e resposta REST. É o que o subprojeto vizinho
  [`http-client`](../http-client/README.md) faz com libcurl + esta mesma biblioteca.
- **IPC / mensageria** — o corpo de uma mensagem AMQP em
  [`rabbitmq`](../rabbitmq/README.md), por exemplo.
- **Salvar estado** — nível de jogo, sessão, cache de resultado.

### ⚠ Quando **não** usar JSON

JSON é texto: legível, mas gordo e caro de parsear. Se o gargalo for volume ou latência —
milhões de mensagens por segundo, telemetria, arquivos de gigabytes — troque por um formato
binário. A própria `nlohmann` serializa para três deles sem trocar de biblioteca
(`to_cbor`, `to_msgpack`, `to_ubjson`), e o **demo 7 mede a diferença**. Para esquemas
estáveis com evolução controlada e ainda menos bytes, o caminho é Protocol Buffers.

E para valores realmente longos, JSON não representa `NaN` nem `Infinity` — a biblioteca
grava `null` no lugar, silenciosamente.

---

## Estrutura de arquivos

```
sandbox/
├── README.md      ← este arquivo
├── main.cpp       ← as 7 demos; é também o rascunho: apague o main() e escreva o seu
└── meson.build    ← alvo `sandbox`, dependência única: json_dep (nlohmann_json 3.11.3)
```

Três arquivos, de propósito. Um rascunho com dez arquivos deixa de ser rascunho.

### Dependências

O alvo depende de **uma** coisa: `nlohmann_json` (versão 3.11.3, resolvida pelo Conan —
veja a tabela de dependências do [README da raiz](../../README.md)). Ela é *header-only*:
não há biblioteca para linkar, o `#include <nlohmann/json.hpp>` traz tudo.

> ⚠ Este alvo já linkava `protobuf`, `spdlog`, `fmt` e os arquivos gerados
> `common_proto_cc`/`common_proto_h` — **nenhum** deles usado pelo `main.cpp`. Dependência
> que não se usa é tempo de build e superfície de quebra de graça. Foram removidas; o
> `meson.build` documenta o porquê.

---

## Os demos, um a um

Rode `./build/src/sandbox/sandbox` e acompanhe. Toda saída citada abaixo foi **copiada da
execução real**, não escrita à mão.

### Demo 1 — construir um `json` e imprimi-lo

O exemplo mínimo, seis linhas:

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

json j;                          // começa como null
j["curso"] = "C++ moderno";      // a atribuição transforma o null em objeto
std::cout << j.dump() << "\n";   // {"curso":"C++ moderno"}
```

O demo completo acrescenta um array de `Aluno` e imprime nos dois formatos:

```
vazio e null?      true
tipo agora:        object
dump() compacto:
{"alunos":[{"idade":25,"nome":"Lucas","nota":9.5},{"idade":22,"nome":"Ana","nota":8.0}],"curso":"C++ moderno"}
```

#### ⚠ A ordem das chaves não é a que você escreveu

Repare: o código insere `"curso"` **primeiro**, e a saída começa por `"alunos"`. Não é bug.
`nlohmann::json` guarda objetos num `std::map<std::string, json>`, que é ordenado — as
chaves saem em **ordem alfabética**, sempre.

Na maioria dos casos tanto faz (JSON não define ordem de chaves como significativa). Quando
importa — um arquivo de configuração que um humano vai ler e comparar com `git diff` — use
o tipo irmão, que troca o `std::map` por um vetor de pares:

```cpp
nlohmann::ordered_json o;
o["curso"]  = "C++ moderno";
o["alunos"] = json::array({1, 2});
// {"curso":"C++ moderno","alunos":[1,2]}   ← ordem de inserção preservada
```

O preço: busca por chave passa de O(log n) para O(n).

### Demo 2 — desserializar uma string

```cpp
json config = json::parse(R"({
    "debug": true,
    "max_alunos": 30,
    "tags": ["cpp", "json"],
    "limite": null
})");
```

`R"( ... )"` é um **raw string literal** (C++11): nada lá dentro precisa de escape, e por
isso o JSON embutido continua legível. Sem ele, cada `"` viraria `\"`. Veja
[`strings`](../strings/README.md).

Saída real:

```
debug (value):     true
max_alunos (get):  30
tags[0]:           cpp
limite e null?     true
items():
  debug = true
  limite = null
  max_alunos = 30
  tags = ["cpp","json"]
```

Duas formas de tirar o valor, e a diferença **importa**:

| Forma | Se a chave falta | Se o tipo não bate |
|---|---|---|
| `j["k"].get<int>()` | insere `null` e depois lança `type_error` | lança `type_error` |
| `j.at("k").get<int>()` | lança `out_of_range` | lança `type_error` |
| `j.value("k", 42)` | devolve `42`, não lança | lança `type_error` |

Para iterar um **objeto** (chave e valor) use `.items()`; para iterar um **array** (só o
valor) o `for` de intervalo normal basta. O `for (const auto& [chave, valor] : ...)` é
*structured binding* (C++17): desmonta o par em duas variáveis nomeadas.

### Demo 3 — `struct` ↔ `json`, com e sem macro

O ponto do demo é que o código de **uso** é o mesmo nos dois caminhos:

```cpp
Aluno a{"Lucas", 25, 9.5};
json  ja = a;                     // chama to_json por ADL
Aluno volta = ja.get<Aluno>();    // chama from_json por ADL
```

O que a macro gera, escrito à mão — são estas oito linhas, e nada mais:

```cpp
// Isto:
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Aluno, nome, idade, nota)

// gera, em essência, isto:
void to_json(json& j, const Aluno& a) {
    j = json{{"nome", a.nome}, {"idade", a.idade}, {"nota", a.nota}};
}
void from_json(const json& j, Aluno& a) {
    j.at("nome").get_to(a.nome);
    j.at("idade").get_to(a.idade);
    j.at("nota").get_to(a.nota);
}
```

Escrever à mão custa oito linhas e compra três coisas que a macro não dá: **renomear
chaves** (`carga_horaria` no JSON, `horas` no C++), **valores padrão** para campos ausentes,
e **validação**. O `Curso` do `main.cpp` mostra os dois primeiros:

```cpp
void from_json(const json& j, Curso& c) {
    j.at("titulo").get_to(c.titulo);          // obrigatório: lança se faltar
    c.carga_horaria = j.value("carga_horaria", 0);  // opcional: 0 se faltar
}
```

```
Aluno -> json:     {"idade":25,"nome":"Lucas","nota":9.5}
json -> Aluno:     Lucas, nota 9.5
Curso -> json:     {"carga_horaria":40,"titulo":"C++ do zero"}
sem carga_horaria: sem carga / 0
```

A mesma entrada sem `nota` faria `Aluno` lançar `out_of_range`, porque a macro usa `.at()`
em todos os campos. É para esse caso que existe `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`.

### Demo 4 — ler e escrever um arquivo

O caso mais comum da vida real, e o mais curto:

```cpp
// escrever
std::ofstream out("config.json");
out << cfg.dump(2) << "\n";

// ler — as duas linhas abaixo são equivalentes
std::ifstream in("config.json");
json lido;  in >> lido;
// json lido = json::parse(in);
```

O demo grava num arquivo temporário (`std::filesystem::temp_directory_path()`), lê de
volta, compara com `==` e apaga:

```
arquivo:           /tmp/cpp_academy_sandbox.json
bytes escritos:    39
round-trip igual?  true
```

`operator==` entre dois `json` compara **valor**, recursivamente — não texto. Dois
documentos com as mesmas chaves em ordem diferente são iguais. Para streams e
`std::filesystem`, veja [`file_io`](../file_io/README.md).

> ⚠ `std::ifstream` que falha ao abrir **não lança** por padrão: você recebe um stream em
> estado de erro, e `in >> lido` então lança um `parse_error` sobre entrada vazia — a
> mensagem fala de JSON, não do arquivo que não existe. Verifique `in.is_open()` antes.

### Demo 5 — os três erros

```cpp
try {
    json descartado = json::parse(R"({"quebrado": )");
    (void)descartado;
} catch (const json::parse_error& e) {
    std::cout << "parse_error id=" << e.id << " byte=" << e.byte << "\n";
}
```

Saída real, com os `id` estáveis e o byte exato onde o parser desistiu:

```
parse_error  id=101 byte=14
type_error   id=302
out_of_range id=403
base exception id=101
is_discarded()     true
```

Para capturar os três de uma vez, pegue a base: `catch (const nlohmann::json::exception& e)`.
Ela expõe `e.id` (o código) e `e.what()` (a mensagem legível).

E existe a saída sem exceção nenhuma, útil quando entrada malformada é rotina e não
excepcional (um socket, por exemplo):

```cpp
json j = json::parse(texto, nullptr, /*allow_exceptions=*/false);
if (j.is_discarded()) { /* texto inválido, nada foi lançado */ }
```

> ⚠ `json::parse` é `[[nodiscard]]`. Chamá-la só pelo efeito colateral do erro, sem guardar
> o retorno, é um warning — e com `warning_level=3` neste repositório, ruído no build.
> Guarde em uma variável e marque `(void)`, como o demo faz.

### Demo 6 — `j["chave"]` **cria** a chave

A armadilha número um da biblioteca, e a que mais custa a aparecer:

```cpp
json j = json::parse(R"({"a": 1})");
(void)j["b"];                 // ← ISTO INSERE "b": null
```

```
antes:             {"a":1}
depois de j["b"]:  {"a":1,"b":null}
contains("c"):     false
value("c", 42):    42
ainda sem "c":     {"a":1,"b":null}
contains("b"):     true  b e null? true
```

Num `json` **não-const**, `operator[]` tem semântica de `std::map::operator[]`: se a chave
não existe, ela é **inserida** com valor `null`. Uma leitura que parecia inofensiva mudou o
documento — e depois você grava o arquivo com um campo que ninguém pediu.

As três alternativas seguras, por intenção:

| Você quer | Use | Se faltar |
|---|---|---|
| exigir a chave | `j.at("k")` | lança `out_of_range` |
| um padrão | `j.value("k", padrão)` | devolve o padrão |
| só saber se existe | `j.contains("k")` | `false` |

E repare na última linha do demo: **`null` não é o mesmo que ausente.** Depois do
`j["b"]`, `contains("b")` é `true` e `j["b"].is_null()` também é `true`. Se o seu código
trata "campo ausente" e "campo nulo" da mesma forma, diga isso explicitamente:
`if (!j.contains("b") || j["b"].is_null())`.

Num `json` **const**, `operator[]` não insere nada — e por isso mesmo é **comportamento
indefinido** se a chave faltar. O tipo de UB que o subprojeto
[`undefined_behavior`](../undefined_behavior/README.md) trata.

### Demo 7 — JSON contra os formatos binários (medido)

Cem `Aluno` serializados de cinco maneiras. Os números vêm da execução real do binário,
não de estimativa:

| Formato | Bytes | Contra o JSON |
|---|---|---|
| JSON (texto, compacto) | **4091** | — |
| CBOR (RFC 8949) | **3132** | −23 % |
| MessagePack | **3093** | −24 % |
| UBJSON | **4192** | +2 % |

```cpp
const std::vector<std::uint8_t> cbor = json::to_cbor(j);
const json                      volta = json::from_cbor(cbor);   // round-trip fiel
```

Duas leituras deste resultado:

1. **A economia é modesta neste dado** porque os `Aluno` têm nomes de chave repetidos
   (`"nome"`, `"idade"`, `"nota"` × 100) e nenhum desses formatos comprime chaves repetidas.
   Quem faz isso é um formato com **esquema**, como Protocol Buffers, onde a chave vira um
   número de campo. A vantagem real do CBOR/MessagePack aqui está no **tempo de parse** e em
   representar números sem convertê-los para texto e de volta.
2. **UBJSON ficou maior que o JSON.** Nem todo formato binário é menor; medir o seu dado é
   obrigatório antes de trocar. É a mesma disciplina de [`performance`](../performance/README.md).

---

## Usando este diretório como rascunho

O outro papel do subprojeto. O que você pode fazer sem quebrar nada:

- **Apagar tudo dentro de `main()`** e escrever o seu experimento. Nenhum outro alvo do
  repositório inclui este arquivo.
- **Apagar as funções de demo** que não interessam — elas estão num `namespace` anônimo,
  ninguém as vê de fora.
- **Acrescentar `#include`s da STL** à vontade: não custa nada ao build.

O que exige tocar no `meson.build`:

- **Usar outra biblioteca.** Acrescente o `*_dep` correspondente à lista `dependencies:`.
  Os nomes disponíveis estão declarados no [`meson.build` da raiz](../../meson.build)
  (`fmt_dep`, `spdlog_dep`, `boost_dep`, ...). O subprojeto
  [`meson`](../meson/README.md) explica a sintaxe.

O que **não** fazer:

- **Renomear o alvo de volta para `main`.** Ele colide com `src/collision/`, que instala
  `dist/bin/main`.
- **Apagar o arquivo.** `src/meson.build` tem `subdir('sandbox')` ativo; sem `main.cpp` a
  configuração falha.

Para voltar ao estado original depois de brincar: `git diff src/sandbox/main.cpp` mostra o
que você mudou, e o histórico do Git tem a versão de referência.

---

## Como validar

```bash
make build
./build/src/sandbox/sandbox
```

Sete blocos, terminando em `fim.`. Os valores que devem bater **exatamente** na sua máquina
(são todos determinísticos, sem relógio, sem aleatório, sem thread):

```
=== Demo 1 - construir um json ===
vazio e null?      true
tipo agora:        object
...
ordered_json:      {"curso":"C++ moderno","alunos":[1,2]}

=== Demo 3 - struct <-> json ===
Aluno -> json:     {"idade":25,"nome":"Lucas","nota":9.5}
sem carga_horaria: sem carga / 0

=== Demo 4 - arquivo ===
bytes escritos:    39
round-trip igual?  true

=== Demo 5 - os tres erros ===
parse_error  id=101 byte=14
type_error   id=302
out_of_range id=403
is_discarded()     true

=== Demo 6 - [] cria a chave; at/value/contains nao ===
depois de j["b"]:  {"a":1,"b":null}

=== Demo 7 - formatos binarios (medido) ===
json (texto):      4091 bytes
cbor:              3132 bytes
msgpack:           3093 bytes
ubjson:            4192 bytes
cbor volta igual?  true
```

O único item que varia é o caminho em `Demo 4 - arquivo`, que vem de
`std::filesystem::temp_directory_path()` (em Linux, `/tmp`).

Confirmar o nome do alvo e o binário instalado:

```bash
grep -n "executable(" src/sandbox/meson.build   # → executable('sandbox', ...)
make install && ls -l dist/bin/sandbox
```

---

## Referências

### Neste repositório

- [`http-client`](../http-client/README.md) — a mesma biblioteca em uso real: JSON como
  corpo de requisição e resposta HTTP com libcurl.
- [`file_io`](../file_io/README.md) — `ifstream`/`ofstream`, `stringstream` e
  `std::filesystem`, usados no demo 4.
- [`errors`](../errors/README.md) — exceções, hierarquia, garantias e `noexcept`; o
  vocabulário do demo 5.
- [`vocabulary_types`](../vocabulary_types/README.md) — `std::variant`, a versão estática da
  ideia de tipo-soma que `nlohmann::json` implementa dinamicamente.
- [`operators`](../operators/README.md) — sobrecarga e ADL, o mecanismo por trás de
  `to_json`/`from_json`.
- [`layout`](../layout/README.md) — por que `memcpy` de uma struct não serve como formato
  de troca.
- [`performance`](../performance/README.md) — como medir antes de trocar de formato.
- [`meson`](../meson/README.md) — a sintaxe do `meson.build` que você vai editar ao trazer
  uma biblioteca nova para o rascunho.

### Fora daqui

- [nlohmann/json — README e documentação](https://github.com/nlohmann/json) — a referência
  principal; veja as seções *Arbitrary types conversions*, *Exceptions* e *Binary formats*.
- [RFC 8259 — The JavaScript Object Notation (JSON) Data Interchange Format](https://www.rfc-editor.org/rfc/rfc8259)
  — a gramática oficial, sete páginas.
- [RFC 8949 — CBOR](https://www.rfc-editor.org/rfc/rfc8949) e
  [MessagePack](https://msgpack.org/) — os formatos binários medidos no demo 7.
- [RFC 6901 — JSON Pointer](https://www.rfc-editor.org/rfc/rfc6901) e
  [RFC 6902 — JSON Patch](https://www.rfc-editor.org/rfc/rfc6902) — suportados pela
  biblioteca (`j["/a/b"_json_pointer]`, `j.patch(...)`), fora do escopo destes demos.
