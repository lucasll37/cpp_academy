#include "db.hpp"

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

// ⚠ O backend PostgreSQL é OPCIONAL neste subprojeto, e a razão é concreta:
//   `soci/postgresql/soci-postgresql.h` inclui `libpq-fe.h`, o cabeçalho do
//   cliente do PostgreSQL. Sem `libpq-dev` instalado no sistema, a compilação
//   falha com `fatal error: libpq-fe.h: No such file or directory`.
//
//   Um subprojeto DIDÁTICO não deve exigir um servidor de banco de dados para
//   compilar. O SQLite é embutido — é um arquivo, sem processo, sem rede — e
//   ensina exatamente as mesmas coisas: sessão, prepared statement, transação,
//   binding de parâmetros, mapeamento de tipos.
//
//   O `meson.build` da raiz declara `soci_pg_dep` com `required: false`, e o
//   deste subprojeto define `TEM_POSTGRESQL` quando ele é encontrado. É o
//   mesmo padrão de detecção graciosa de src/cmake/ para MPI e CUDA.
//
//   Para habilitar:  sudo apt install libpq-dev  (e reconfigure)
#if defined(TEM_POSTGRESQL) && TEM_POSTGRESQL
#  include <soci/postgresql/soci-postgresql.h>
#endif

#include <stdexcept>

namespace db {

// ─── SociRow : adapta soci::row → db::Row ────────────────────────────────────

class SociRow : public Row {
    const soci::row& row_;
public:
    explicit SociRow(const soci::row& r) : row_(r) {}

    std::string get_string(const std::string& col) const override {
        return row_.get<std::string>(col);
    }

    int get_int(const std::string& col) const override {
        // SOCI armazena inteiros como int ou long long dependendo do backend
        try {
            return row_.get<int>(col);
        } catch (...) {
            return static_cast<int>(row_.get<long long>(col));
        }
    }

    double get_double(const std::string& col) const override {
        return row_.get<double>(col);
    }

    bool is_null(const std::string& col) const override {
        return row_.get_indicator(col) == soci::i_null;
    }
};

// ─── SociConnection ───────────────────────────────────────────────────────────

class SociConnection : public Connection {
    soci::session session_;

    // O DSN escolhe o backend em RUNTIME. É este ponto — e só ele — que
    // conhece backends concretos; todo o resto do arquivo fala com a interface
    // `soci::session`. Trocar de banco é trocar a string de conexão.
    static soci::backend_factory const& pick_backend(const std::string& dsn) {
        if (dsn.rfind("sqlite3://", 0) == 0) return soci::sqlite3;

#if defined(TEM_POSTGRESQL) && TEM_POSTGRESQL
        if (dsn.rfind("postgresql://", 0) == 0) return soci::postgresql;
        if (dsn.rfind("postgres://", 0)   == 0) return soci::postgresql;
        throw DbError("DSN nao reconhecido. Use sqlite3:// ou postgresql://");
#else
        if (dsn.rfind("postgresql://", 0) == 0 || dsn.rfind("postgres://", 0) == 0)
            throw DbError(
                "DSN PostgreSQL pedido, mas este binario foi compilado SEM o "
                "backend PostgreSQL (falta libpq-dev). Use sqlite3:// ou "
                "instale libpq-dev e reconfigure.");
        throw DbError("DSN nao reconhecido. Use sqlite3://");
#endif
    }

    static std::string strip_prefix(const std::string& dsn) {
        auto pos = dsn.find("://");
        return (pos != std::string::npos) ? dsn.substr(pos + 3) : dsn;
    }

public:
    explicit SociConnection(const std::string& dsn)
        : session_(pick_backend(dsn), strip_prefix(dsn))
    {}

    // ── execute ──────────────────────────────────────────────────────────────
    void execute(const std::string& sql) override {
        try {
            session_ << sql;
        } catch (const soci::soci_error& e) {
            throw DbError(e.what());
        }
    }

    // ── query ────────────────────────────────────────────────────────────────
    void query(const std::string& sql,
               std::function<void(const Row&)> callback) override {
        try {
            soci::rowset<soci::row> rs = (session_.prepare << sql);
            for (const auto& r : rs) {
                SociRow row(r);
                callback(row);
            }
        } catch (const soci::soci_error& e) {
            throw DbError(e.what());
        }
    }

    // ── transaction ──────────────────────────────────────────────────────────
    void transaction(std::function<void()> fn) override {
        soci::transaction tx(session_);
        try {
            fn();
            tx.commit();
        } catch (...) {
            tx.rollback();
            throw;
        }
    }

    // ── execute_prepared ─────────────────────────────────────────────────────
    // Bind posicional: substitui :1 :2 ... pelos valores fornecidos
    int execute_prepared(const std::string& sql,
                         const std::vector<std::string>& params) override {
        try {
            soci::statement st(session_);
            st.alloc();
            st.prepare(sql);

            // Bind cada parâmetro como string (SOCI faz a coerção internamente)
            std::vector<soci::indicator> inds(params.size(), soci::i_ok);
            for (std::size_t i = 0; i < params.size(); ++i)
                st.exchange(soci::use(params[i], inds[i]));

            st.define_and_bind();
            st.execute(true);
            return static_cast<int>(st.get_affected_rows());
        } catch (const soci::soci_error& e) {
            throw DbError(e.what());
        }
    }
};

// ─── Factory ─────────────────────────────────────────────────────────────────

std::unique_ptr<Connection> connect(const std::string& dsn) {
    return std::make_unique<SociConnection>(dsn);
}

} // namespace db