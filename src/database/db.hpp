#pragma once

#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <memory>
#include <optional>

namespace db {

// ─── Erros ────────────────────────────────────────────────────────────────────

struct DbError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ─── Row ─────────────────────────────────────────────────────────────────────
// Representa uma linha de resultado. Acesso por nome de coluna.

class Row {
public:
    virtual ~Row() = default;

    virtual std::string         get_string (const std::string& col) const = 0;
    virtual int                 get_int    (const std::string& col) const = 0;
    virtual double              get_double (const std::string& col) const = 0;
    virtual bool                is_null    (const std::string& col) const = 0;
};

// ─── Connection ───────────────────────────────────────────────────────────────

class Connection {
public:
    virtual ~Connection() = default;

    // DDL / DML sem retorno de linhas
    virtual void execute(const std::string& sql) = 0;

    // Executa query e chama callback para cada linha
    virtual void query(const std::string& sql,
                       std::function<void(const Row&)> callback) = 0;

    // Transação: executa fn dentro de BEGIN/COMMIT, faz ROLLBACK em exceção
    virtual void transaction(std::function<void()> fn) = 0;

    // Prepared statement com bind por posição (usa :1, :2 ... ou $1, $2 ...)
    // Retorna número de linhas afetadas
    virtual int  execute_prepared(const std::string& sql,
                                  const std::vector<std::string>& params) = 0;
};

// ─── Factory ──────────────────────────────────────────────────────────────────

// DSN examples:
//   SQLite  → "sqlite3://./academy.db"
//   PG      → "postgresql://user:pass@localhost/mydb"
std::unique_ptr<Connection> connect(const std::string& dsn);

} // namespace db
