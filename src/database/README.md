# database — SQL a partir do C++, sem casar com o banco

## O que é este projeto?

Um CRUD completo sobre SQLite, escrito atrás de uma **interface abstrata de 4 métodos**
(`db::Connection`) e da biblioteca [SOCI](https://soci.sourceforge.net/). O programa cria uma
tabela de produtos, insere, consulta, atualiza, debita estoque dentro de uma **transação** e
apaga — e nenhuma linha da lógica de negócio sabe qual banco está por baixo.

```bash
make build
./dist/bin/database
```

O binário chama-se `database` e fica em `./dist/bin/database`. Ele grava um arquivo de banco em
`tmp/sqlite/academy.db`, **na raiz do repositório** — e é seguro rodar quantas vezes você
quiser: o resultado é sempre o mesmo (ver [O `DROP TABLE`](#o-drop-table-e-por-que-ele-está-ali)).
Para compilar só este alvo: `meson compile -C build database`.

---

## Antes de começar: tudo o que este README vai usar

Esta seção não pressupõe **nada** de banco de dados. Se você nunca escreveu SQL, leia-a até o
fim; nada mais adiante depende de conhecimento externo.

### 1. Banco, tabela, linha, coluna

Um **banco relacional** guarda dados em **tabelas**. Tabela é uma planilha com tipo: as
**colunas** têm nome e tipo fixos, e cada **linha** (ou *registro*) é um item.

```
                     tabela  products
        ┌──────┬──────────────────┬─────────┬─────────┐
colunas │  id  │       name       │  price  │  stock  │   ← nome + tipo
        ├──────┼──────────────────┼─────────┼─────────┤
linha 1 │  1   │ Teclado Mecânico │  349.90 │   15    │
linha 2 │  2   │ Mouse Gamer      │  189.99 │   30    │
linha 3 │  3   │ Monitor 27"      │ 1899.00 │    8    │
        └──────┴──────────────────┴─────────┴─────────┘
           ▲
           └── chave primária: identifica a linha de forma única
```

A **chave primária** (`PRIMARY KEY`) identifica cada linha sem ambiguidade — duas linhas nunca
podem ter a mesma. É por ela que se diz "atualize o produto 3", e não "atualize o produto cujo
nome é Monitor 27 polegadas e cujo preço é 1899".

### 2. SQL, e os quatro verbos

**SQL** (*Structured Query Language*) é a linguagem que se fala com o banco: você manda uma
string, ele devolve linhas ou um número. São quatro verbos, e juntos formam a sigla **CRUD**.

| Verbo SQL | CRUD | Exemplo sobre `products` |
|---|---|---|
| `INSERT` | **C**reate | `INSERT INTO products (name, price, stock) VALUES ('Mouse', 189.99, 30)` |
| `SELECT` | **R**ead | `SELECT id, name FROM products WHERE stock > 0` |
| `UPDATE` | **U**pdate | `UPDATE products SET price = 1699.00 WHERE id = 3` |
| `DELETE` | **D**elete | `DELETE FROM products WHERE id = 4` |

Três palavras coladas nesses comandos: **`WHERE cond`** filtra (só as linhas em que `cond` é
verdadeira — **sem `WHERE`, o `UPDATE` e o `DELETE` valem para a tabela inteira**, o erro mais
caro que existe em SQL); **`ORDER BY col`** ordena (sem ele a ordem **não é garantida**); e
**`SET col = valor`** é o que o `UPDATE` muda.

### 3. DDL e DML — as duas metades do SQL

| Sigla | Significa | Mexe em | Comandos |
|---|---|---|---|
| **DDL** | *Data **Definition** Language* | a **forma** da tabela | `CREATE TABLE`, `DROP TABLE`, `ALTER TABLE` |
| **DML** | *Data **Manipulation** Language* | o **conteúdo** | `INSERT`, `SELECT`, `UPDATE`, `DELETE` |

DDL desenha a planilha; DML preenche as células. É a divisão que explica por que
`db::Connection` tem um `execute()` (o que não devolve linhas) e um `query()` separado.

### 4. O `CREATE TABLE` deste projeto, palavra por palavra

Este é o SQL exato de [main.cpp](main.cpp) linhas 48-55, com quatro restrições (*constraints*)
— regras que o **banco** garante e que nenhum bug de C++ consegue furar:

```sql
CREATE TABLE IF NOT EXISTS products (
    id    INTEGER PRIMARY KEY AUTOINCREMENT,
    name  TEXT    NOT NULL UNIQUE,
    price REAL    NOT NULL CHECK(price >= 0),
    stock INTEGER NOT NULL DEFAULT 0
)
```

| Pedaço | O que significa |
|---|---|
| `IF NOT EXISTS` | não é erro se a tabela já existir — só não faz nada |
| `INTEGER`, `TEXT`, `REAL` | os tipos do SQLite: inteiro, texto, ponto flutuante |
| `PRIMARY KEY` | identificador único da linha |
| `AUTOINCREMENT` | o banco escolhe o próximo `id` sozinho — por isso o `INSERT` não passa `id` |
| `NOT NULL` | proibido ficar sem valor |
| `UNIQUE` | não pode haver duas linhas com o mesmo `name` |
| `CHECK(price >= 0)` | o banco **recusa** um preço negativo, venha de onde vier |
| `DEFAULT 0` | se o `INSERT` não citar `stock`, entra `0` |

> Constraint é regra de negócio guardada no lugar mais defensável que existe — **junto do
> dado**. Cem programas podem escrever nessa tabela; todos os cem obedecem ao `CHECK`.

### 5. `NULL` não é `0`, nem string vazia

`NULL` é o terceiro estado de uma coluna: **"não há valor aqui"** — não é zero (um número
conhecido) nem `""` (um texto conhecido, de tamanho zero). É a ausência.

```
stock = 0      →  "eu sei que o estoque é zero"
stock = NULL   →  "eu não sei qual é o estoque"
```

Por isso a interface tem [`Row::is_null()`](db.hpp): perguntar `get_int("stock")` numa célula
`NULL` é uma **pergunta mal formulada** — não existe `int` que represente "não sei".

```cpp
if (row.is_null("stock")) { /* trate a ausência */ }
else                      { int s = row.get_int("stock"); }
```

> ⚠ Armadilha de SQL junto: comparações com `NULL` **nunca dão verdadeiro**. `WHERE stock =
> NULL` não retorna nada, nem mesmo as linhas com `NULL` — a forma certa é `WHERE stock IS
> NULL`. As colunas deste projeto são todas `NOT NULL`, então `is_null()` nunca dispara aqui;
> o método existe porque uma camada de acesso a banco que não sabe dizer "vazio" está quebrada.

### 6. Prepared statement e *bind* — a única defesa real contra injection

Concatenar SQL com dados do usuário é a vulnerabilidade mais antiga da web:

```cpp
// ✗ NUNCA
std::string sql = "SELECT * FROM users WHERE name = '" + nome + "'";
```

Se `nome` for `x' OR '1'='1`, a string vira `... WHERE name = 'x' OR '1'='1'` e o `WHERE` deixa
de filtrar. Se for `x'; DROP TABLE users; --`, pior. Um **prepared statement** separa estrutura
e valor em **duas viagens**:

```
1. PREPARE  "INSERT INTO products (name, price, stock) VALUES (:1, :2, :3)"
            └── só a ESTRUTURA vai ao banco; ele faz o parse e monta o plano

2. BIND     :1 ← "Mouse Gamer"      :2 ← "189.99"      :3 ← "30"
            └── os VALORES vão depois, por um canal que não é texto SQL

3. EXECUTE
```

Aquele `:1` é um **placeholder** (marcador de posição), e **fazer o bind** é preenchê-lo com um
valor. Como o parse já terminou no passo 1, nenhum caractere do valor pode virar sintaxe —
aspas, `;` e `--` chegam como *conteúdo*. Não é uma escapada de aspas melhor: é uma via separada.

O segundo ganho está no **plano de query**: a rota que o banco escolhe para executar o comando
("varrer a tabela toda" vs "usar o índice em `name`"). Calculá-lo custa tempo, e o statement
preparado uma vez pode ser executado mil vezes com valores diferentes reaproveitando o plano.

### 7. Transação, e o "A" de ACID

Uma **transação** é um grupo de comandos que o banco trata como **um só**. A propriedade que
importa aqui é a **atomicidade**: *ou todas as operações do grupo valem, ou nenhuma vale* — não
existe meio-termo. O caso clássico é o débito de estoque de um pedido com dois itens:

```
   sem transação                       com transação
─────────────────────────         ─────────────────────────
UPDATE teclado -2   ✓ ok          BEGIN
UPDATE mouse   -5   ✗ falha       UPDATE teclado -2   ✓ ok
                                  UPDATE mouse   -5   ✗ falha
   estado final:                  ROLLBACK    ◀── desfaz TUDO
   teclado debitado,
   mouse não.                        estado final:
   O banco mente.                    exatamente como antes.
```

| Palavra | Significado |
|---|---|
| `BEGIN` | abre a transação — daqui em diante nada é definitivo |
| `COMMIT` | fecha confirmando: tudo o que foi feito passa a valer, de uma vez |
| `ROLLBACK` | fecha desfazendo: o banco volta a ser o que era no `BEGIN` |
| **atomicidade** | o A de ACID: nunca metade |

ACID tem mais três letras — **C**onsistência (as constraints continuam valendo no fim),
**I**solamento (uma transação não enxerga o meio de outra) e **D**urabilidade (depois do
`COMMIT`, sobrevive a queda de energia). Este subprojeto exercita o **A**.

### 8. SOCI, backend e DSN

**SOCI** é uma biblioteca C++ de acesso a banco — não *é* um banco, é uma camada fina e uniforme
por cima dos clientes nativos de vários deles. Cada cliente é um **backend** (`soci_sqlite3`,
`soci_postgresql`, `soci_mysql`, …): o núcleo `soci_core` define a API, o backend fala o
protocolo real.

```
   seu código  ──▶  soci::session  ──▶  backend  ──▶  banco
                    (API única)      sqlite3.so      arquivo .db
                                     libpq.so        servidor TCP
```

O **DSN** (*Data Source Name*) é a string que diz **qual backend e onde**. O esquema (antes do
`://`) escolhe o backend; o resto é interpretado **pelo backend** — para o SQLite, é apenas um
caminho de arquivo:

```
sqlite3:///caminho/do/repo/tmp/sqlite/academy.db

postgresql://usuario:senha@localhost:5432/meu_banco
└────┬────┘   └──┬───┘ └─┬─┘ └───┬───┘ └┬┘ └───┬───┘
  esquema     usuário  senha   host   porta  banco
```

Em [db.cpp](db.cpp) linhas 66-86, `pick_backend()` olha só o esquema e `strip_prefix()` entrega
o resto ao backend — **é o único ponto do programa que conhece bancos concretos.**

### 9. SQLite × PostgreSQL — a diferença que muda o DSN

| | SQLite | PostgreSQL |
|---|---|---|
| forma | **biblioteca embutida** no seu processo | **servidor** separado, falado por TCP |
| onde o dado mora | um arquivo `.db` | um diretório gerido pelo servidor |
| para começar | nada a instalar, nada a subir | instalar, iniciar o serviço, criar usuário e banco |
| concorrência | um escritor por vez | muitos escritores, controle fino |
| uso típico | app de celular, cache local, **teste**, este subprojeto | produção multiusuário |

O SQLite ensina tudo o que interessa aqui — sessão, prepared statement, transação, bind,
mapeamento de tipos — sem exigir um servidor.

### 10. As quatro palavras de C++ que a interface usa

Aparecem todas nas 60 linhas de [db.hpp](db.hpp):

- **Interface abstrata** — classe sem dado e sem implementação, que existe só para declarar *o
  que se pode pedir*. `db::Connection` é uma.
- **Função virtual pura** — o `= 0` no fim da declaração: "toda classe concreta que herdar daqui
  **tem de** implementar isto". Torna a classe impossível de instanciar diretamente.
- **`std::unique_ptr<T>`** — ponteiro dono: destrói o objeto ao sair de escopo, sem `delete`
  escrito à mão. É o que `db::connect()` devolve, e é por isso que a conexão fecha sozinha,
  inclusive se uma exceção subir. Ver [../smart-pointers/README.md](../smart-pointers/README.md).
- **Factory** — função que decide **qual** classe concreta criar e devolve a interface. Quem
  chama `db::connect(dsn)` recebe `unique_ptr<Connection>` e nunca ouve falar de `SociConnection`.

> ⚠ Repare no destrutor: `virtual ~Connection() = default;`. Sem o `virtual`, destruir a classe
> derivada através de um ponteiro para a base é **comportamento indefinido**. Vale para `Row`
> e para `Connection`, e é por isso que os dois o declaram.

---

## Por que estudar isto?

| Sem esta camada | Com esta camada |
|---|---|
| você inclui `sqlite3.h` e o programa **casa** com o SQLite | trocar de banco é trocar o DSN |
| SQL montado com `+` — injection à espera | `execute_prepared` com bind |
| cada erro é um `int` de retorno que dá para esquecer de checar | `DbError` é exceção: ignorar não é uma opção |
| o `sqlite3_close()` esquecido num `return` antecipado vaza | `unique_ptr` fecha sempre, inclusive na exceção |
| dois `UPDATE`s podem deixar o estoque pela metade | `transaction()` garante tudo-ou-nada |
| testar exige o banco de produção | o teste roda contra um `.db` descartável |

Os três motivos, um por frase: **(1) inversão de dependência** — `ProductRepository` depende de
`db::Connection` (abstração), não de SOCI nem de SQLite, que é o D do SOLID aplicado
([../solid/README.md](../solid/README.md)); **(2) prepared statement é a única defesa real**
contra SQL injection, porque escapar aspas à mão é uma corrida que se perde; **(3) transação é
o que impede um sistema de dinheiro ou estoque de ficar inconsistente**, e não há como emular
isso do lado do C++.

---

## Estrutura de arquivos

```
src/database/
├── README.md      ← este arquivo
├── db.hpp         ← a interface: DbError, Row, Connection, connect()  (60 linhas)
├── db.cpp         ← a implementação SOCI: SociRow, SociConnection, factory
├── main.cpp       ← Product, ProductRepository e as 6 etapas do demo
└── meson.build    ← executable('database') + detecção opcional do PostgreSQL
```

`db.hpp` é o arquivo a ler primeiro.

---

## A interface `db::Connection`

O conteúdo real de [db.hpp](db.hpp), sem cortes:

```cpp
namespace db {

// Toda falha vira ESTA exceção — o chamador nunca vê soci::soci_error.
struct DbError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Uma linha de resultado. Acesso por NOME de coluna, não por índice.
class Row {
public:
    virtual ~Row() = default;                                        // ver ⚠ acima

    virtual std::string get_string(const std::string& col) const = 0;
    virtual int         get_int   (const std::string& col) const = 0;
    virtual double      get_double(const std::string& col) const = 0;
    virtual bool        is_null   (const std::string& col) const = 0;
};

class Connection {
public:
    virtual ~Connection() = default;

    // DDL / DML que NÃO devolve linhas. SQL literal, sem parâmetros.
    virtual void execute(const std::string& sql) = 0;

    // SELECT. Chama o callback UMA VEZ POR LINHA, enquanto lê.
    virtual void query(const std::string& sql,
                       std::function<void(const Row&)> callback) = 0;

    // Roda fn() entre BEGIN e COMMIT; qualquer exceção vira ROLLBACK.
    virtual void transaction(std::function<void()> fn) = 0;

    // Prepared statement com bind posicional (:1, :2, ...).
    // Retorna o NÚMERO DE LINHAS AFETADAS.
    virtual int  execute_prepared(const std::string& sql,
                                  const std::vector<std::string>& params) = 0;
};

// Factory: escolhe o backend a partir do DSN.
std::unique_ptr<Connection> connect(const std::string& dsn);

} // namespace db
```

Quatro métodos, e cada um existe por um motivo:

| Método | Por que é separado |
|---|---|
| `execute` | não há linhas para devolver; e o SQL é literal, escrito por você |
| `query` | devolve linhas — e o **callback** as processa **em fluxo**, sem materializar um vetor de milhões de linhas na memória |
| `transaction` | recebe um lambda porque o `ROLLBACK` precisa acontecer **em torno** dele |
| `execute_prepared` | tem valores vindos de fora; é o único que aceita parâmetros |

> `query()` com callback é *streaming*: o `soci::rowset` de [db.cpp](db.cpp) linha 106 é um
> iterador preguiçoso. Quem quiser um `std::vector` monta um — é o que `find_all()` faz.

---

## Exemplo mínimo

Antes de qualquer padrão de projeto, o programa inteiro cabe em dez linhas úteis:

```cpp
#include "db.hpp"
#include <fmt/core.h>

int main() {
    auto conn = db::connect("sqlite3:///tmp/exemplo.db");   // unique_ptr: fecha sozinho

    conn->execute("DROP TABLE IF EXISTS gatos");
    conn->execute("CREATE TABLE gatos (id INTEGER PRIMARY KEY, nome TEXT NOT NULL)");
    conn->execute_prepared("INSERT INTO gatos (nome) VALUES (:1)", {"Salem"});
    conn->execute_prepared("INSERT INTO gatos (nome) VALUES (:1)", {"Mingau"});
    conn->query("SELECT id, nome FROM gatos ORDER BY id", [](const db::Row& r) {
        fmt::print("{} — {}\n", r.get_int("id"), r.get_string("nome"));
    });
}
```

Não há `new`, `delete` nem `close()`: o `unique_ptr` fecha a conexão ao sair do `main`,
inclusive se um `DbError` for lançado no meio.

---

## Os demos, um a um

`main()` ([main.cpp](main.cpp) linhas 140-209) roda seis etapas em ordem. Cada uma corresponde
a um método de `ProductRepository`.

### 0 · Onde o banco é gravado

```cpp
auto db_dir = fs::canonical(fs::read_symlink("/proc/self/exe"))
                  .parent_path()   // dist/bin/
                  .parent_path()   // dist/
                  .parent_path()   // raiz do projeto
              / "tmp" / "sqlite";
fs::create_directories(db_dir);
const std::string dsn = "sqlite3://" + (db_dir / "academy.db").string();
```

> ⚠ **É `<raiz-do-repositório>/tmp/sqlite/academy.db`, não o `/tmp` do sistema.** `/tmp` é
> limpo pelo sistema operacional; `./tmp` é pasta do projeto, e o banco persiste entre
> execuções. O caminho vem de `/proc/self/exe` — o link que o Linux mantém para o executável do
> próprio processo — e não do diretório de trabalho, para funcionar chamado de qualquer lugar.

### 1 · `CREATE TABLE` — DDL

`repo.create_table()` roda **dois** comandos: um `DROP TABLE IF EXISTS products` e depois o
`CREATE TABLE IF NOT EXISTS`. Ver a seção seguinte, que é só sobre isso.

```
-- CREATE TABLE ------------------------------
  tabela 'products' criada
```

### 2 · `INSERT` — prepared statement, cinco vezes

```cpp
void insert(const Product& p) {
    conn_.execute_prepared(
        "INSERT INTO products (name, price, stock) VALUES (:1, :2, :3)",
        {p.name, std::to_string(p.price), std::to_string(p.stock)}
    );
}
```

O `id` não aparece: quem o gera é o `AUTOINCREMENT`. Os cinco produtos incluem, de propósito,
um **Headset USB com estoque 0** — ele torna visível o filtro da etapa 4 e é o alvo do `DELETE`.

```
-- INSERT ------------------------------------
  5 produtos inseridos
```

> ⚠ **Todos os parâmetros viajam como `std::string`**, e o SOCI coage para o tipo da coluna do
> outro lado. É uma simplificação deliberada da API, com um custo real: `std::to_string(double)`
> usa **6 casas decimais fixas**. Medido: `349.90` → `"349.900000"` ✓, mas `1.0/3.0` →
> `"0.333333"` (perdeu precisão) e `1e-7` → `"0.000000"` (**virou zero**). Para preços em reais
> é inofensivo; para dado científico, é corrupção silenciosa. A evolução natural da API é
> sobrecarregar `execute_prepared` com tipos, deixando o `soci::use` levar o `double` nativo.

### 3 · `SELECT *` — `query()` com callback

```cpp
conn_.query("SELECT id, name, price, stock FROM products ORDER BY id",
    [&](const db::Row& row) {
        result.push_back({ row.get_int("id"),    row.get_string("name"),
                           row.get_double("price"), row.get_int("stock") });
    });
```

Acesso **por nome** de coluna, não por índice: mudar a ordem no `SELECT` não quebra nada.

```
-- SELECT * ----------------------------------
  ID   Nome                    Preco  Estoque
  --------------------------------------------
  1    Teclado Mecânico     R$ 349.90      15
  2    Mouse Gamer          R$ 189.99      30
  3    Monitor 27"          R$1899.00       8
  4    Headset USB          R$ 249.00       0
  5    Webcam 1080p         R$ 159.50      12
```

### 4 · `SELECT ... WHERE stock > 0` — o filtro

Mesma consulta com `WHERE`: o Headset (id 4) some. Note que o `ORDER BY` também sumiu — a ordem
sai coincidentemente crescente, mas **não é garantida**.

```
-- SELECT WHERE stock > 0 --------------------
  1    Teclado Mecânico     R$ 349.90      15
  2    Mouse Gamer          R$ 189.99      30
  3    Monitor 27"          R$1899.00       8
  5    Webcam 1080p         R$ 159.50      12
```

### 5 · `UPDATE` — o Monitor cai de R$ 1899,00 para R$ 1699,00

```cpp
conn_.execute_prepared("UPDATE products SET price = :1 WHERE id = :2",
                       {std::to_string(new_price), std::to_string(id)});
```

```
-- UPDATE price (Monitor → R$1.699,00) -----
  3    Monitor 27"          R$1699.00       8      ← era 1899.00
```

> ⚠ O `WHERE id = :2` é o que impede este comando de mudar **o preço de todos os produtos**.

### 6 · `TRANSACTION` — debitar estoque de dois produtos, atomicamente

```cpp
void debit_stock(const std::vector<std::pair<int,int>>& items) {
    conn_.transaction([&]() {
        for (auto& [id, qty] : items) {
            conn_.execute_prepared(
                "UPDATE products SET stock = stock - :1 WHERE id = :2 AND stock >= :1",
                {std::to_string(qty), std::to_string(id)});
        }
    });
}
```

A implementação em [db.cpp](db.cpp) linhas 117-126 é o padrão inteiro em nove linhas:

```cpp
void transaction(std::function<void()> fn) override {
    soci::transaction tx(session_);   // construtor emite BEGIN
    try {
        fn();
        tx.commit();                  // COMMIT explícito
    } catch (...) {
        tx.rollback();                // qualquer exceção → desfaz tudo
        throw;                        // e RELANÇA: ver ⚠
    }
}
```

> ⚠ O `throw;` sem operando **relança a exceção original**, preservando tipo e mensagem —
> trocá-lo por `throw DbError("falhou")` destruiria a informação de *o que* falhou. E o
> `catch (...)` pega tudo, inclusive exceções nascidas no código do chamador dentro do lambda:
> transação tem de reverter em **qualquer** saída anormal. `soci::transaction` também é RAII —
> se algo escapasse antes do `commit()`, o destrutor faria o rollback assim mesmo.

O `WHERE ... AND stock >= :1` é uma **guarda**: o estoque nunca fica negativo, porque a
condição impede o `UPDATE` de atingir a linha. É o banco garantindo a regra, não o C++.

```
-- TRANSACTION debit_stock -------------------
  debitando: Teclado x2, Mouse x5
  1    Teclado Mecânico     R$ 349.90      13      ← 15 − 2
  2    Mouse Gamer          R$ 189.99      25      ← 30 − 5
```

> ⚠ **O `:1` aparece duas vezes no SQL, e há só dois valores no vetor. Funciona — mas por um
> motivo de backend que você precisa conhecer antes de copiar.** O backend SQLite do SOCI
> entrega a query **crua** ao `sqlite3_prepare_v2`, sem reescrever placeholders; quem
> interpreta `:1`/`:2` é o próprio SQLite, e a regra dele é *parâmetro nomeado repetido recebe
> sempre o mesmo índice*. Logo há dois parâmetros: `:1` → 1 (as duas ocorrências), `:2` → 2. O
> `st.exchange(soci::use(...))` de [db.cpp](db.cpp) linha 140 faz bind **por posição**, e casa.
>
> A consequência que morde: **os números dentro dos `:N` têm de estar em ordem crescente de
> primeira aparição no texto do SQL**, porque quem manda é a posição do bind, não o número que
> você escreveu — `... WHERE id = :2 AND qty = :1` receberia os valores trocados. E outro
> backend pode tratar placeholder repetido de outra forma. Prefira um valor por placeholder.

### 7 · `DELETE` — o Headset sem estoque

```
-- DELETE (Headset sem estoque) --------------
  1    Teclado Mecânico     R$ 349.90      13
  2    Mouse Gamer          R$ 189.99      25
  3    Monitor 27"          R$1699.00       8
  5    Webcam 1080p         R$ 159.50      12
```

O `id` 4 desapareceu, e o 5 **não** foi renumerado: chave primária é identidade, não posição.

---

## O `DROP TABLE`, e por que ele está ali

[main.cpp](main.cpp) linha 46 executa `conn_.execute("DROP TABLE IF EXISTS products")` antes
do `CREATE`.

> ⚠ **Sem essa linha, o demo só funcionava na primeira execução.** O `CREATE TABLE IF NOT
> EXISTS` preservava as cinco linhas da rodada anterior, o primeiro `INSERT` batia na constraint
> `UNIQUE` de `name` e o programa morria com `DbError: UNIQUE constraint failed: products.name`,
> saindo com código 1. Um exemplo didático que só funciona uma vez é um exemplo quebrado. Com o
> `DROP`, o programa é **idempotente**: rodar dez vezes dá exatamente o mesmo estado final.

> ⚠ **Em produção isso é inaceitável — `DROP TABLE` apaga os dados.** Lá o instrumento é
> **migração**: uma sequência versionada de scripts (`V1__cria_products.sql`,
> `V2__add_coluna_sku.sql`, …), cada um aplicado **uma única vez** e registrado numa tabela de
> controle. Ferramentas: **Flyway** e **Liquibase** (JVM, agnósticas de linguagem) e **Alembic**
> (Python/SQLAlchemy). O princípio é o do controle de versão: o esquema passa a ter histórico.

---

## Trocar de banco sem mudar código — e o que trava aqui

Em teoria, todo o programa acima roda contra PostgreSQL trocando uma string:
`db::connect("postgresql://user:senha@localhost/academy")`.

> ⚠ **Neste build, isso lança uma exceção.** O alvo é compilado com `-DTEM_POSTGRESQL=0` e
> [db.cpp](db.cpp) linhas 74-78 respondem:
>
> ```
> DbError: DSN PostgreSQL pedido, mas este binario foi compilado SEM o backend
>          PostgreSQL (falta libpq-dev). Use sqlite3:// ou instale libpq-dev e
>          reconfigure.
> ```

São **dois** motivos independentes, documentados em [meson.build](meson.build) e no
[meson.build](../../meson.build) da raiz (linhas 31-34):

1. O arquivo `.pc` que o Conan gera para `soci-soci_postgresql` traz um
   `Requires: soci-soci_core none` malformado — o `none` é um bug do gerador — e o `pkg-config`
   rejeita o pacote inteiro por causa dele.
2. Mesmo achando o pacote, `soci-postgresql.h` inclui `libpq-fe.h`, o cabeçalho do cliente
   PostgreSQL. Sem `libpq-dev` no sistema, a compilação morre com
   `fatal error: libpq-fe.h: No such file or directory`.

A detecção é graciosa, e o idioma vale como padrão — `has_header()` de fato **compila um
programinha de teste** para descobrir, em vez de presumir:

```meson
cc_db  = meson.get_compiler('cpp')
tem_pg = soci_pg_dep.found() and cc_db.has_header('libpq-fe.h')
cpp_args: tem_pg ? ['-DTEM_POSTGRESQL=1'] : ['-DTEM_POSTGRESQL=0']
```

Para habilitar: `sudo apt install libpq-dev`, depois `make configure` (procure a linha
`database: backend PostgreSQL HABILITADO`) e `make build`. Um subprojeto didático não deve
exigir um servidor de banco para compilar — por isso o SQLite é o caminho padrão.

---

## Como validar

```bash
# 1. compilar e rodar
make build
./dist/bin/database
```

Devem sair as etapas na ordem: `CREATE TABLE`, `INSERT`, `SELECT *`, `SELECT WHERE stock > 0`,
`UPDATE price`, `TRANSACTION debit_stock`, `DELETE` — com Monitor a `R$1699.00`, Teclado `13`,
Mouse `25`, e o `id` 4 ausente no fim.

```bash
# 2. idempotência: rodar de novo tem de dar EXATAMENTE o mesmo resultado
./dist/bin/database > /tmp/a.txt && ./dist/bin/database > /tmp/b.txt
diff /tmp/a.txt /tmp/b.txt && echo "idempotente ✓"

# 3. o efeito real no arquivo de banco — a prova mais convincente
ls -l tmp/sqlite/academy.db
```

Com o cliente instalado (`sudo apt install sqlite3`): `sqlite3 tmp/sqlite/academy.db ".schema
products"`. Sem ele, o Python da distribuição já traz o módulo:

```bash
python3 - <<'EOF'
import sqlite3
c = sqlite3.connect('tmp/sqlite/academy.db')
print(c.execute("SELECT sql FROM sqlite_master WHERE name='products'").fetchone()[0])
for r in c.execute("SELECT * FROM products"): print(r)
EOF
```

Saída medida — o `CREATE TABLE` sai igualzinho ao do código, indentação inclusa:

```
(1, 'Teclado Mecânico', 349.9, 13)
(2, 'Mouse Gamer', 189.99, 25)
(3, 'Monitor 27"', 1699.0, 8)
(5, 'Webcam 1080p', 159.5, 12)
```

Note que `349.90` voltou como `349.9` (é `REAL`, um `double` — zero à direita não existe em binário) e que o `id` 4 realmente não está lá.

### 4. Sentir as constraints: tente violá-las

```bash
python3 - <<'EOF'
import sqlite3
c = sqlite3.connect('tmp/sqlite/academy.db')
for sql in ["INSERT INTO products (name, price) VALUES ('Teste', -1)",
            "INSERT INTO products (name, price) VALUES ('Mouse Gamer', 1)"]:
    try:    c.execute(sql)
    except Exception as e: print('recusado:', e)
c.rollback()
EOF
```

Saída medida — o primeiro bate no `CHECK`, o segundo no `UNIQUE`. Nenhum código C++ foi
consultado: quem recusou foi o banco.

```
recusado: CHECK constraint failed: price >= 0
recusado: UNIQUE constraint failed: products.name
```

---

## Referências

### Neste repositório

- [../solid/README.md](../solid/README.md) — inversão de dependência: `ProductRepository`
  depende de `db::Connection`, não de SOCI
- [../smart-pointers/README.md](../smart-pointers/README.md) — o `unique_ptr` que fecha a conexão
- [../errors/README.md](../errors/README.md) — por que `DbError` é exceção, não código de retorno
- [../design-of-pattern/README.md](../design-of-pattern/README.md) — Factory e Adapter, usados em
  `db.cpp` (`SociRow` adapta `soci::row` para `db::Row`)
- [../meson/README.md](../meson/README.md) — `dependency(required: false)` e `has_header()`
- [db.hpp](db.hpp) · [db.cpp](db.cpp) · [main.cpp](main.cpp) · [meson.build](meson.build)

### Fontes canônicas

- SOCI — <https://soci.sourceforge.net/doc/master/>
- SQLite — sintaxe SQL — <https://www.sqlite.org/lang.html> · parâmetros —
  <https://www.sqlite.org/lang_expr.html#varparam> · datatypes —
  <https://www.sqlite.org/datatype3.html>
- PostgreSQL — <https://www.postgresql.org/docs/current/>
- OWASP — SQL Injection Prevention Cheat Sheet —
  <https://cheatsheetseries.owasp.org/cheatsheets/SQL_Injection_Prevention_Cheat_Sheet.html>
- Flyway — <https://documentation.red-gate.com/flyway> · Liquibase —
  <https://docs.liquibase.com/> · Alembic — <https://alembic.sqlalchemy.org/>
