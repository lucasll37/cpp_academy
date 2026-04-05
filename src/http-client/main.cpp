#include "http_client.hpp"

#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/color.h>

#include <vector>

using json = nlohmann::json;

// ─── Modelo de domínio ───────────────────────────────────────────────────────

struct Todo {
    int         id;
    int         userId;
    std::string title;
    bool        completed;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Todo, id, userId, title, completed)

// ─── helpers ─────────────────────────────────────────────────────────────────

void print_todo(const Todo& t) {
    auto color = t.completed ? fmt::color::green : fmt::color::orange;
    auto icon  = t.completed ? "✓" : "○";

    fmt::print("  [{:>3}] ", t.id);
    fmt::print(fg(color), "{} ", icon);
    fmt::print("{}\n", t.title);
}

void print_separator(const std::string& title) {
    fmt::print(fmt::emphasis::bold, "\n── {} ", title);
    fmt::print("{}\n", std::string(40 - title.size(), '─'));
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {
    // ── 1. GET único recurso ────────────────────────────────────────────────
    print_separator("GET /todos/1");
    try {
        auto resp = http::get("https://jsonplaceholder.typicode.com/todos/1");

        fmt::print("status       : {}\n", resp.status_code);
        fmt::print("content-type : {}\n", resp.headers.count("content-type")
                                        ? resp.headers.at("content-type")
                                        : "(ausente)");

        Todo todo = json::parse(resp.body).get<Todo>();
        print_todo(todo);

    } catch (const http::HttpError& e) {
        fmt::print(stderr, "erro de transporte: {}\n", e.what());
        return 1;
    }

    // ── 2. GET lista + filtro ───────────────────────────────────────────────
    print_separator("GET /todos?userId=1");
    try {
        auto resp = http::get("https://jsonplaceholder.typicode.com/todos?userId=1");
        fmt::print("status: {}\n\n", resp.status_code);

        auto todos = json::parse(resp.body).get<std::vector<Todo>>();

        std::vector<Todo> done;
        for (auto& t : todos)
            if (t.completed) done.push_back(t);

        fmt::print("Total    : {}\n", todos.size());
        fmt::print("Concluídas: {}\n\n", done.size());

        for (auto& t : done) print_todo(t);

    } catch (const http::HttpError& e) {
        fmt::print(stderr, "erro de transporte: {}\n", e.what());
        return 1;
    }

    // ── 3. Header customizado ───────────────────────────────────────────────
    print_separator("GET /posts/1 com header");
    try {
        auto resp = http::get(
            "https://jsonplaceholder.typicode.com/posts/1",
            {{"X-Custom-Header", "cpp_academy"}}
        );

        fmt::print("status: {}\n", resp.status_code);

        auto post = json::parse(resp.body);
        fmt::print("título: {}\n", post["title"].get<std::string>());

    } catch (const http::HttpError& e) {
        fmt::print(stderr, "erro de transporte: {}\n", e.what());
        return 1;
    }

    // ── 4. 404 ─────────────────────────────────────────────────────────────
    print_separator("GET /todos/9999 (404)");
    try {
        auto resp = http::get("https://jsonplaceholder.typicode.com/todos/9999");
        fmt::print("status: {}\n", resp.status_code);
        fmt::print("body  : {}\n", resp.body);

    } catch (const http::HttpError& e) {
        fmt::print(stderr, "erro de transporte: {}\n", e.what());
        return 1;
    }

    fmt::print("\n");
    return 0;
}
