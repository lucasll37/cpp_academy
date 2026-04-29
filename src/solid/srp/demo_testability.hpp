#pragma once
// =============================================================================
//  demo_testability.hpp — SRP e testabilidade: interfaces + injeção
// =============================================================================
//
//  Uma das consequências mais imediatas do SRP é a testabilidade.
//  Classes com responsabilidade única:
//    • Dependem de menos coisas → menos mocks necessários
//    • Têm contratos bem definidos → testes fáceis de escrever
//    • Mudam por uma só razão → testes não quebram por razões não relacionadas
//
//  Este demo mostra como a combinação de SRP + interfaces abstratas
//  + injeção de dependência (DI) torna o código 100% testável sem
//  tocar em banco de dados, rede ou sistema de arquivos.
//
//  Padrão: Dependency Inversion Principle (o 'D' do SOLID)
//  funciona como alavanca que viabiliza o SRP na prática.
//
// =============================================================================

#include <fmt/core.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <sstream>

namespace demo_testability {

// ─────────────────────────────────────────────────────────────────────────────
//  Domínio: sistema de autenticação
// ─────────────────────────────────────────────────────────────────────────────
struct User {
    std::string id;
    std::string email;
    std::string password_hash;
    bool        active;
};

struct LoginResult {
    bool        success;
    std::string token;
    std::string message;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Interfaces abstratas — cada uma representa uma responsabilidade
// ─────────────────────────────────────────────────────────────────────────────

// Responsabilidade: buscar usuários (pode ser DB, LDAP, cache...)
struct IUserRepository {
    virtual ~IUserRepository() = default;
    virtual std::optional<User> find_by_email(const std::string& email) const = 0;
};

// Responsabilidade: verificar senhas (pode ser bcrypt, argon2, plaintext em testes)
struct IPasswordHasher {
    virtual ~IPasswordHasher() = default;
    virtual bool verify(const std::string& raw, const std::string& hash) const = 0;
};

// Responsabilidade: gerar tokens de sessão (JWT, UUID, Redis...)
struct ITokenGenerator {
    virtual ~ITokenGenerator() = default;
    virtual std::string generate(const std::string& user_id) const = 0;
};

// Responsabilidade: registrar eventos de autenticação (audit log)
struct IAuditLogger {
    virtual ~IAuditLogger() = default;
    virtual void log_login(const std::string& email, bool success) const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  AuthService — responsabilidade única: orquestrar o fluxo de autenticação
//  Não implementa NADA — delega tudo para os colaboradores injetados.
// ─────────────────────────────────────────────────────────────────────────────
class AuthService {
public:
    AuthService(std::shared_ptr<IUserRepository> repo,
                std::shared_ptr<IPasswordHasher> hasher,
                std::shared_ptr<ITokenGenerator> tokens,
                std::shared_ptr<IAuditLogger>    audit)
        : repo_(std::move(repo))
        , hasher_(std::move(hasher))
        , tokens_(std::move(tokens))
        , audit_(std::move(audit))
    {}

    LoginResult login(const std::string& email,
                      const std::string& password) const
    {
        auto user = repo_->find_by_email(email);
        if (!user) {
            audit_->log_login(email, false);
            return {false, "", "user not found"};
        }

        if (!user->active) {
            audit_->log_login(email, false);
            return {false, "", "account disabled"};
        }

        if (!hasher_->verify(password, user->password_hash)) {
            audit_->log_login(email, false);
            return {false, "", "invalid password"};
        }

        std::string token = tokens_->generate(user->id);
        audit_->log_login(email, true);
        return {true, token, "ok"};
    }

private:
    std::shared_ptr<IUserRepository> repo_;
    std::shared_ptr<IPasswordHasher> hasher_;
    std::shared_ptr<ITokenGenerator> tokens_;
    std::shared_ptr<IAuditLogger>    audit_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Implementações REAIS (produção)
// ─────────────────────────────────────────────────────────────────────────────
class BcryptHasher : public IPasswordHasher {
public:
    bool verify(const std::string& raw, const std::string& hash) const override {
        // Em produção: bcrypt::verify(raw, hash)
        // Simulado: hash = "bcrypt:" + raw
        return hash == "bcrypt:" + raw;
    }
};

class JwtTokenGenerator : public ITokenGenerator {
public:
    std::string generate(const std::string& user_id) const override {
        // Em produção: jwt::encode({sub: user_id}, secret)
        return "jwt." + user_id + ".signed";
    }
};

class ConsoleAuditLogger : public IAuditLogger {
public:
    void log_login(const std::string& email, bool success) const override {
        fmt::print("  [AUDIT] login {} — {}\n",
                   success ? "OK  " : "FAIL", email);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Implementações de TESTE (mocks/stubs — sem I/O, sem banco)
//  Em um projeto real ficam em tests/ e usam GTest/GMock.
//  Aqui ficam inline para o demo ser autocontido.
// ─────────────────────────────────────────────────────────────────────────────
class InMemoryUserRepository : public IUserRepository {
public:
    void add(User u) { users_.push_back(std::move(u)); }

    std::optional<User> find_by_email(const std::string& email) const override {
        for (const auto& u : users_)
            if (u.email == email) return u;
        return std::nullopt;
    }
private:
    std::vector<User> users_;
};

// Stub de hasher: aceita sempre "correct-password"
class StubHasher : public IPasswordHasher {
public:
    bool verify(const std::string& raw, const std::string&) const override {
        return raw == "correct-password";
    }
};

// Stub de token: sempre retorna um token fixo (fácil de assertar no teste)
class StubTokenGenerator : public ITokenGenerator {
public:
    std::string generate(const std::string&) const override {
        return "test-token-12345";
    }
};

// Spy de audit: grava as chamadas para assertar depois
class SpyAuditLogger : public IAuditLogger {
public:
    void log_login(const std::string& email, bool success) const override {
        calls_.push_back({email, success});
    }

    struct Call { std::string email; bool success; };
    mutable std::vector<Call> calls_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  "Testes" inline — demonstram que cada classe é testável isoladamente
// ─────────────────────────────────────────────────────────────────────────────
static void assert_eq(bool got, bool expected, const std::string& label) {
    bool ok = (got == expected);
    fmt::print("  [{}] {}\n", ok ? "PASS" : "FAIL", label);
}

static void assert_eq(const std::string& got, const std::string& expected,
                      const std::string& label)
{
    bool ok = (got == expected);
    fmt::print("  [{}] {} (got='{}')\n", ok ? "PASS" : "FAIL", label, got);
}

void demo_tests() {
    fmt::print("\n  Testes isolados — sem banco, sem rede, sem arquivo:\n\n");

    // ── Setup: repositório em memória ─────────────────────────────────────
    auto repo   = std::make_shared<InMemoryUserRepository>();
    auto hasher = std::make_shared<StubHasher>();
    auto tokens = std::make_shared<StubTokenGenerator>();
    auto audit  = std::make_shared<SpyAuditLogger>();

    repo->add({"u-001", "alice@ex.com", "hash", true});
    repo->add({"u-002", "bob@ex.com",   "hash", false}); // inativo

    AuthService auth{repo, hasher, tokens, audit};

    // ── Teste 1: login bem-sucedido ───────────────────────────────────────
    {
        auto r = auth.login("alice@ex.com", "correct-password");
        assert_eq(r.success, true,   "login válido → success=true");
        assert_eq(r.token, "test-token-12345", "token retornado corretamente");
    }

    // ── Teste 2: senha errada ─────────────────────────────────────────────
    {
        auto r = auth.login("alice@ex.com", "wrong-password");
        assert_eq(r.success, false, "senha errada → success=false");
        assert_eq(r.message, "invalid password", "mensagem correta");
    }

    // ── Teste 3: usuário não existe ───────────────────────────────────────
    {
        auto r = auth.login("nao@existe.com", "qualquer");
        assert_eq(r.success, false, "usuário inexistente → success=false");
        assert_eq(r.message, "user not found", "mensagem correta");
    }

    // ── Teste 4: conta desativada ─────────────────────────────────────────
    {
        auto r = auth.login("bob@ex.com", "correct-password");
        assert_eq(r.success, false, "conta inativa → success=false");
        assert_eq(r.message, "account disabled", "mensagem correta");
    }

    // ── Teste 5: verificar que o audit logger foi chamado ─────────────────
    {
        bool audit_logged_failure = false;
        for (const auto& call : audit->calls_)
            if (!call.success && call.email == "alice@ex.com")
                audit_logged_failure = true;
        assert_eq(audit_logged_failure, true, "audit logou falha de alice");
    }

    fmt::print("\n  → SpyAuditLogger registrou {} chamadas:\n",
               audit->calls_.size());
    for (const auto& c : audit->calls_)
        fmt::print("    email={:25s} success={}\n", c.email, c.success);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo com implementações de produção
// ─────────────────────────────────────────────────────────────────────────────
void demo_production() {
    fmt::print("\n  Modo produção (implementações reais):\n");

    auto repo   = std::make_shared<InMemoryUserRepository>();
    auto hasher = std::make_shared<BcryptHasher>();
    auto tokens = std::make_shared<JwtTokenGenerator>();
    auto audit  = std::make_shared<ConsoleAuditLogger>();

    repo->add({"u-001", "alice@ex.com", "bcrypt:secret123", true});

    AuthService auth{repo, hasher, tokens, audit};

    auto r = auth.login("alice@ex.com", "secret123");
    fmt::print("  login result: success={} token='{}'\n",
               r.success, r.token);
}

inline void run() {
    fmt::print("┌─ Demo 5: SRP + Interfaces + Testabilidade ──────────────┐\n");
    demo_tests();
    demo_production();
    fmt::print("\n");
    fmt::print("  Trocar BcryptHasher por Argon2Hasher:\n");
    fmt::print("    → zero mudanças em AuthService, Repository ou Logger\n");
    fmt::print("  Trocar DB por Redis cache:\n");
    fmt::print("    → apenas nova implementação de IUserRepository\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_testability
