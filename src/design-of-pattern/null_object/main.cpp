// =============================================================================
//  null_object/main.cpp
// =============================================================================
#include "null_object.hpp"
#include <fmt/core.h>
#include <fmt/color.h>

#include <memory>

using namespace patterns;

// =============================================================================
//  Demo 1 — O problema: código cheio de nullptr checks
// =============================================================================
void demo_problema() {
    fmt::print("── 1. O problema — null checks espalhados pelo código ──\n\n");

    // Sem Null Object, o serviço precisaria de guards em todo lugar:
    fmt::print("  Sem Null Object (pseudocódigo):\n");
    fmt::print("    if (logger_) logger_->info(\"iniciando\");\n");
    fmt::print("    // ... lógica ...\n");
    fmt::print("    if (logger_) logger_->error(\"falhou\");\n");
    fmt::print("    if (notifier_) notifier_->send(user, msg);\n\n");

    fmt::print("  Com Null Object:\n");
    fmt::print("    logger_.info(\"iniciando\");   // sempre seguro\n");
    fmt::print("    logger_.error(\"falhou\");     // sempre seguro\n");
    fmt::print("    notifier_.send(user, msg);   // sempre seguro\n\n");

    fmt::print("  A diferença: zero condicionais no código cliente.\n\n");
}

// =============================================================================
//  Demo 2 — Logger: real vs null, mesma interface
// =============================================================================
void demo_logger() {
    fmt::print("── 2. Null Logger — logging opcional sem if ──\n\n");

    fmt::print("  Com ConsoleLogger (produção / debug):\n");
    {
        ConsoleLogger logger;
        PaymentService svc(logger);
        svc.charge("alice@example.com", 150.00);
    }

    fmt::print("\n  Com NullLogger (testes / silencioso):\n");
    {
        NullLogger logger; // nenhuma linha impressa pelo serviço
        PaymentService svc(logger);
        bool ok = svc.charge("bob@example.com", 200.00);
        fmt::print("  (sem output de log — mas retornou: {})\n", ok);
    }

    fmt::print("\n  Com NullLogger — valor inválido (erro silencioso):\n");
    {
        NullLogger logger;
        PaymentService svc(logger);
        bool ok = svc.charge("carol@example.com", -50.0);
        fmt::print("  retornou: {} (erro silenciado pelo NullLogger)\n", ok);
    }
}

// =============================================================================
//  Demo 3 — Notifier: troca em runtime, sem mudar PaymentService
// =============================================================================
void demo_notifier() {
    fmt::print("\n── 3. Null Strategy — notificador opcional ──\n\n");

    // Serviço simples que usa um notificador
    auto notify_order = [](INotifier& notifier,
                           const std::string& user,
                           const std::string& msg) {
        // Não verifica se notifier é "válido" — sempre chama
        notifier.send(user, msg);
        if (notifier.channel() != "null")
            fmt::print("  [OrderService] notificação enviada via {}\n",
                       notifier.channel());
        else
            fmt::print("  [OrderService] notificações desativadas\n");
    };

    EmailNotifier email;
    SMSNotifier   sms;
    NullNotifier  nulo;

    fmt::print("  Com Email:\n  ");
    notify_order(email, "alice@example.com", "Pedido #42 confirmado");

    fmt::print("  Com SMS:\n  ");
    notify_order(sms, "+55-11-99999-0000", "Pedido #43 confirmado");

    fmt::print("  Com Null (sem canal configurado):\n  ");
    notify_order(nulo, "user", "Pedido #44 confirmado");
}

// =============================================================================
//  Demo 4 — Null Object vs std::optional
// =============================================================================
void demo_vs_optional() {
    fmt::print("\n── 4. Null Object vs std::optional — quando usar cada um ──\n\n");

    InMemoryUserRepository repo;

    fmt::print("  Buscando usuários (com Null Object sentinela):\n");
    for (int id : {1, 2, 99, 42}) {
        User u = repo.find(id);
        if (u.id == UNKNOWN_USER.id) {
            fmt::print("  id={} → não encontrado\n", id);
        } else {
            fmt::print("  id={} → {} <{}>\n", id, u.name, u.email);
        }
    }

    fmt::print("\n  Comparação:\n");
    fmt::print("    std::optional<User>:\n");
    fmt::print("      ✓ Semântica clara de \"pode não existir\"\n");
    fmt::print("      ✓ Força o caller a lidar com ausência (has_value())\n");
    fmt::print("      ✗ Requer check explícito antes de usar\n\n");
    fmt::print("    Null Object (User sentinela):\n");
    fmt::print("      ✓ Zero checks — código cliente mais limpo\n");
    fmt::print("      ✓ Polimórfico — funciona com herança\n");
    fmt::print("      ✗ Caller pode ignorar que é um null (acidente)\n\n");
    fmt::print("    Regra: use optional para dados, Null Object para comportamento.\n");
}

int main() {
    demo_problema();
    demo_logger();
    demo_notifier();
    demo_vs_optional();

    fmt::print(fmt::emphasis::bold,
               "\n✓ Null Object: zero nullptr checks, mesmo comportamento.\n\n");
}
