// =============================================================================
//  null_object/null_object.hpp
//
//  Null Object — eliminar verificações de nulo com polimorfismo
//  ──────────────────────────────────────────────────────────────
//  Substitui `if (ptr != nullptr) ptr->do_something()` por um objeto
//  que implementa a mesma interface mas não faz nada (ou faz o mínimo).
//
//  Não é GoF original, mas é complemento natural de Factory/Strategy
//  e amplamente usado em C++ moderno como alternativa a nullptr checks.
//
//  Benefício: código cliente mais limpo, sem condicionais espalhados.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace patterns {

// ─────────────────────────────────────────────────────────────────────────────
//  Interface — Logger de exemplo
// ─────────────────────────────────────────────────────────────────────────────
struct ILogger {
    virtual ~ILogger() = default;
    virtual void info(const std::string& msg)    = 0;
    virtual void warning(const std::string& msg) = 0;
    virtual void error(const std::string& msg)   = 0;
    virtual bool is_enabled() const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Logger real
// ─────────────────────────────────────────────────────────────────────────────
struct ConsoleLogger : ILogger {
    void info(const std::string& msg)    override { fmt::print("  [INFO]  {}\n", msg); }
    void warning(const std::string& msg) override { fmt::print("  [WARN]  {}\n", msg); }
    void error(const std::string& msg)   override { fmt::print("  [ERROR] {}\n", msg); }
    bool is_enabled() const              override { return true; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Null Logger — faz nada, mas tem a mesma interface
//  O cliente usa ILogger& sem saber se é real ou null
// ─────────────────────────────────────────────────────────────────────────────
struct NullLogger : ILogger {
    void info(const std::string&)    override {} // no-op
    void warning(const std::string&) override {} // no-op
    void error(const std::string&)   override {} // no-op
    bool is_enabled() const          override { return false; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Serviço que usa ILogger — não tem if(logger != nullptr) em lugar nenhum
// ─────────────────────────────────────────────────────────────────────────────
class PaymentService {
public:
    explicit PaymentService(ILogger& logger) : logger_(logger) {}

    bool charge(const std::string& user, double amount) {
        logger_.info(fmt::format("Iniciando cobrança: {} - R${:.2f}", user, amount));

        if (amount <= 0) {
            logger_.error("Valor inválido");
            return false;
        }

        logger_.info("Processando pagamento...");
        // ... lógica real ...
        logger_.info(fmt::format("Cobrança aprovada para {}", user));
        return true;
    }

private:
    ILogger& logger_; // pode ser ConsoleLogger ou NullLogger — não importa
};

// ─────────────────────────────────────────────────────────────────────────────
//  Exemplo 2 — Null Strategy
//
//  Um sistema de notificação pode ou não ter um canal de envio configurado.
//  Em vez de checar `if (notifier_) notifier_->send(...)` em todo lugar,
//  usamos NullNotifier como padrão.
// ─────────────────────────────────────────────────────────────────────────────
struct INotifier {
    virtual ~INotifier() = default;
    virtual void send(const std::string& recipient,
                      const std::string& message) = 0;
    virtual std::string channel() const = 0;
};

struct EmailNotifier : INotifier {
    void send(const std::string& to, const std::string& msg) override {
        fmt::print("  [Email → {}] {}\n", to, msg);
    }
    std::string channel() const override { return "email"; }
};

struct SMSNotifier : INotifier {
    void send(const std::string& to, const std::string& msg) override {
        fmt::print("  [SMS → {}] {}\n", to, msg);
    }
    std::string channel() const override { return "sms"; }
};

// Null: silencioso — o serviço funciona normalmente sem notificações
struct NullNotifier : INotifier {
    void send(const std::string&, const std::string&) override {} // no-op
    std::string channel() const override { return "null"; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Exemplo 3 — Null Object via std::optional (C++17)
//
//  Em C++ moderno, std::optional<T> é uma alternativa ao Null Object
//  para tipos de valor (não polimórficos). Mostramos a comparação.
// ─────────────────────────────────────────────────────────────────────────────
struct User {
    int         id;
    std::string name;
    std::string email;
};

// Sem Null Object: retorna optional — cliente precisa checar
// std::optional<User> find_user_opt(int id);

// Com Null Object: retorna User especial que representa "não encontrado"
struct IUserRepository {
    virtual ~IUserRepository() = default;
    virtual User find(int id) const = 0;
    virtual bool exists(int id) const = 0;
};

// Usuário sentinela — representa "não encontrado" sem nullptr
inline const User UNKNOWN_USER = { -1, "Unknown", "" };

struct InMemoryUserRepository : IUserRepository {
    std::unordered_map<int, User> users_ = {
        {1, {1, "Alice", "alice@example.com"}},
        {2, {2, "Bob",   "bob@example.com"}},
    };

    User find(int id) const override {
        auto it = users_.find(id);
        return it != users_.end() ? it->second : UNKNOWN_USER;
    }

    bool exists(int id) const override { return users_.count(id) > 0; }
};

} // namespace patterns
