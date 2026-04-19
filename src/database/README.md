# database — Abstração de Banco de Dados com SOCI

## O que é este projeto?

Demonstra como criar uma **camada de abstração de banco de dados** em C++ usando a biblioteca **SOCI**, que suporta SQLite, PostgreSQL, MySQL e outros backends. O projeto implementa o padrão **Repository** com prepared statements, transações e proteção contra SQL injection.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| Interface de abstração | `db::Connection` — o código de negócio não conhece o banco específico |
| Repository Pattern | `ProductRepository` isola todo o SQL do resto da aplicação |
| Prepared Statements | Previnem SQL injection e reutilizam planos de query |
| Transações | Operações atômicas com rollback automático em caso de erro |
| DSN (Data Source Name) | String de conexão portável: `sqlite3://path/to/db.sqlite` |

---

## Estrutura de arquivos

```
database/
├── main.cpp       ← ProductRepository + demonstração CRUD completo
├── db.hpp         ← interface db::Connection e db::Row
├── db.cpp         ← implementação via SOCI (SociConnection)
└── meson.build
```

---

## Arquitetura

```
main.cpp
   │
   └── ProductRepository      ← só conhece db::Connection (interface)
              │
              └── db::Connection   ← interface abstrata
                        │
                        └── SociConnection  ← implementação com SOCI
                                  │
                                  ├── SQLite backend
                                  └── PostgreSQL backend
```

A troca de banco de dados não requer nenhuma mudança em `ProductRepository` ou `main.cpp` — apenas no DSN passado para `db::connect()`.

---

## A interface `db::Connection`

```cpp
// db.hpp
namespace db {

struct Row {
    virtual int         get_int(const std::string& col) const = 0;
    virtual double      get_double(const std::string& col) const = 0;
    virtual std::string get_string(const std::string& col) const = 0;
};

class Connection {
public:
    virtual ~Connection() = default;

    // Executa SQL sem retorno (DDL, INSERT, UPDATE, DELETE)
    virtual void execute(const std::string& sql) = 0;

    // Executa com parâmetros — EVITA SQL INJECTION
    virtual void execute_prepared(
        const std::string& sql,
        const std::vector<std::string>& params) = 0;

    // Query com callback por linha
    virtual void query(
        const std::string& sql,
        std::function<void(const Row&)> callback) = 0;

    // Executa função dentro de uma transação
    // → commit automático no sucesso, rollback em exceção
    virtual void transaction(std::function<void()> fn) = 0;
};

// Factory: cria conexão a partir do DSN
std::unique_ptr<Connection> connect(const std::string& dsn);

} // namespace db
```

---

## O Repository Pattern

```cpp
class ProductRepository {
    db::Connection& conn_;

public:
    explicit ProductRepository(db::Connection& c) : conn_(c) {}

    void create_table() {
        conn_.execute(R"(
            CREATE TABLE IF NOT EXISTS products (
                id    INTEGER PRIMARY KEY AUTOINCREMENT,
                name  TEXT    NOT NULL UNIQUE,
                price REAL    NOT NULL CHECK(price >= 0),
                stock INTEGER NOT NULL DEFAULT 0
            )
        )");
    }

    // Prepared statement: parâmetros são :1, :2, :3
    // O SOCI nunca interpreta os valores como SQL → imune a injection
    void insert(const Product& p) {
        conn_.execute_prepared(
            "INSERT INTO products (name, price, stock) VALUES (:1, :2, :3)",
            {p.name, std::to_string(p.price), std::to_string(p.stock)}
        );
    }

    std::vector<Product> find_all() {
        std::vector<Product> result;
        conn_.query(
            "SELECT id, name, price, stock FROM products ORDER BY id",
            [&](const db::Row& row) {
                result.push_back({
                    row.get_int("id"),
                    row.get_string("name"),
                    row.get_double("price"),
                    row.get_int("stock"),
                });
            }
        );
        return result;
    }
};
```

---

## Transações — Atomicidade

```cpp
// Debita estoque de múltiplos produtos de forma atômica
// Se qualquer UPDATE falhar, TODOS são revertidos (rollback)
void debit_stock(const std::vector<std::pair<int,int>>& items) {
    conn_.transaction([&]() {
        for (auto& [id, qty] : items) {
            conn_.execute_prepared(
                "UPDATE products SET stock = stock - :1 "
                "WHERE id = :2 AND stock >= :1",
                {std::to_string(qty), std::to_string(id)}
            );
        }
    });
    // Se execute_prepared lançar exceção → rollback automático
    // Se tudo OK → commit automático
}
```

---

## SQL Injection — Por que Prepared Statements?

### Sem prepared statement (perigoso):
```cpp
// NUNCA faça isso!
std::string name = get_user_input(); // "' OR '1'='1"
conn_.execute("SELECT * FROM products WHERE name = '" + name + "'");
// Query resultante: SELECT * FROM products WHERE name = '' OR '1'='1'
// → retorna TODOS os produtos!
```

### Com prepared statement (seguro):
```cpp
conn_.execute_prepared(
    "SELECT * FROM products WHERE name = :1",
    {name}  // name é passado como dado, nunca como SQL
);
// O banco trata name como texto literal, não como SQL
```

---

## DSN — Trocando de banco sem mudar código

```cpp
// SQLite (arquivo local)
auto conn = db::connect("sqlite3:///tmp/sqlite/academy.db");

// PostgreSQL (servidor remoto)
auto conn = db::connect(
    "postgresql://host=localhost dbname=academy user=postgres password=secret"
);

// O ProductRepository funciona igual com qualquer um!
ProductRepository repo(*conn);
```

---

## Demonstração CRUD completa

```
-- CREATE TABLE ------
  tabela 'products' criada

-- INSERT ------
  5 produtos inseridos

-- SELECT * ------
  ID   Nome                  Preco  Estoque
  ----------------------------------------
  1    Teclado Mecânico     R$349.90     15
  2    Mouse Gamer          R$189.99     30
  3    Monitor 27"         R$1899.00      8
  4    Headset USB          R$249.00      0   ← sem estoque (vermelho)
  5    Webcam 1080p         R$159.50     12

-- TRANSACTION debit_stock ------
  debitando: Teclado x2, Mouse x5
  → Teclado: 15 - 2 = 13
  → Mouse:   30 - 5 = 25
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja database
./bin/database
# banco criado em: ./tmp/sqlite/academy.db
```

---

## Dependências

- `soci` com backend `sqlite3` (e opcionalmente `postgresql`)
- `fmt` — formatação de saída
