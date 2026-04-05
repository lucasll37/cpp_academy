#include "db.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

#include <filesystem>
#include <vector>
#include <string>

// ─── Modelo ──────────────────────────────────────────────────────────────────

struct Product {
    int         id    = 0;
    std::string name;
    double      price = 0.0;
    int         stock = 0;
};

// ─── Repository ──────────────────────────────────────────────────────────────
// Isola o SQL do resto do código. Só conhece db::Connection — funciona igual
// com SQLite e PostgreSQL sem nenhuma mudança.

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

    // INSERT via prepared statement — evita SQL injection
    void insert(const Product& p) {
        conn_.execute_prepared(
            "INSERT INTO products (name, price, stock) VALUES (:1, :2, :3)",
            {p.name, std::to_string(p.price), std::to_string(p.stock)}
        );
    }

    std::vector<Product> find_all() {
        std::vector<Product> result;
        conn_.query("SELECT id, name, price, stock FROM products ORDER BY id",
            [&](const db::Row& row) {
                result.push_back({
                    row.get_int("id"),
                    row.get_string("name"),
                    row.get_double("price"),
                    row.get_int("stock"),
                });
            });
        return result;
    }

    std::vector<Product> find_in_stock() {
        std::vector<Product> result;
        conn_.query("SELECT id, name, price, stock FROM products WHERE stock > 0",
            [&](const db::Row& row) {
                result.push_back({
                    row.get_int("id"),
                    row.get_string("name"),
                    row.get_double("price"),
                    row.get_int("stock"),
                });
            });
        return result;
    }

    void update_price(int id, double new_price) {
        conn_.execute_prepared(
            "UPDATE products SET price = :1 WHERE id = :2",
            {std::to_string(new_price), std::to_string(id)}
        );
    }

    void delete_by_id(int id) {
        conn_.execute_prepared(
            "DELETE FROM products WHERE id = :1",
            {std::to_string(id)}
        );
    }

    // Demonstra transação: debita estoque de vários produtos atomicamente
    void debit_stock(const std::vector<std::pair<int,int>>& items) {
        conn_.transaction([&]() {
            for (auto& [id, qty] : items) {
                conn_.execute_prepared(
                    "UPDATE products SET stock = stock - :1 WHERE id = :2 AND stock >= :1",
                    {std::to_string(qty), std::to_string(id)}
                );
            }
        });
    }
};

// ─── helpers de exibição ─────────────────────────────────────────────────────

void print_header(const std::string& title) {
    fmt::print(fmt::emphasis::bold, "\n-- {} {}\n", title,
               std::string(std::max(0, 42 - (int)title.size()), '-'));
}

void print_products(const std::vector<Product>& products) {
    fmt::print("  {:<4} {:<20} {:>8}  {:>6}\n", "ID", "Nome", "Preco", "Estoque");
    fmt::print("  {}\n", std::string(44, '-'));
    for (auto& p : products) {
        auto stock_color = p.stock > 0 ? fmt::color::green : fmt::color::red;
        fmt::print("  {:<4} {:<20} R${:>7.2f}  ", p.id, p.name, p.price);
        fmt::print(fg(stock_color), "{:>6}\n", p.stock);
    }
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {
    namespace fs = std::filesystem;

    // Deriva o path do banco relativo ao executável:
    // dist/bin/database → ../../tmp/sqlite/academy.db
    auto db_dir  = fs::canonical(fs::read_symlink("/proc/self/exe"))
                       .parent_path()   // dist/bin/
                       .parent_path()   // dist/
                       .parent_path()   // raiz do projeto
                   / "tmp" / "sqlite";

    fs::create_directories(db_dir);    // idempotente, cria toda a árvore

    const std::string dsn = "sqlite3://" + (db_dir / "academy.db").string();

    try {
        auto conn = db::connect(dsn);
        ProductRepository repo(*conn);

        // ── DDL ────────────────────────────────────────────────────────────
        print_header("CREATE TABLE");
        repo.create_table();
        fmt::print("  tabela 'products' criada\n");

        // ── INSERT ─────────────────────────────────────────────────────────
        print_header("INSERT");
        std::vector<Product> seed = {
            {0, "Teclado Mecânico",  349.90, 15},
            {0, "Mouse Gamer",       189.99, 30},
            {0, "Monitor 27\"",     1899.00,  8},
            {0, "Headset USB",       249.00,  0},   // sem estoque
            {0, "Webcam 1080p",      159.50, 12},
        };
        for (auto& p : seed) repo.insert(p);
        fmt::print("  {} produtos inseridos\n", seed.size());

        // ── SELECT todos ───────────────────────────────────────────────────
        print_header("SELECT *");
        print_products(repo.find_all());

        // ── SELECT filtrado ────────────────────────────────────────────────
        print_header("SELECT WHERE stock > 0");
        print_products(repo.find_in_stock());

        // ── UPDATE ─────────────────────────────────────────────────────────
        print_header("UPDATE price (Monitor → R$1.699,00)");
        repo.update_price(3, 1699.00);
        print_products(repo.find_all());

        // ── TRANSACTION: debitar estoque de dois produtos ──────────────────
        print_header("TRANSACTION debit_stock");
        fmt::print("  debitando: Teclado x2, Mouse x5\n");
        repo.debit_stock({{1, 2}, {2, 5}});
        print_products(repo.find_all());

        // ── DELETE ─────────────────────────────────────────────────────────
        print_header("DELETE (Headset sem estoque)");
        repo.delete_by_id(4);
        print_products(repo.find_all());

    } catch (const db::DbError& e) {
        fmt::print(stderr, "DbError: {}\n", e.what());
        return 1;
    } catch (const std::exception& e) {
        fmt::print(stderr, "erro: {}\n", e.what());
        return 1;
    }

    fmt::print("\n");
    return 0;
}