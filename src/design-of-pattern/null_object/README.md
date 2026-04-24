# null_object — Padrão Null Object

## O que é este projeto?

Implementa o padrão **Null Object**: uma implementação da interface que não faz nada (ou faz o mínimo seguro), eliminando verificações de `nullptr` espalhadas pelo código cliente.

Não é GoF original, mas é complemento direto de Factory, Strategy e qualquer padrão que injete dependências.

---

## O problema que resolve

```cpp
// Sem Null Object — null checks em todo lugar
class OrderService {
    ILogger*    logger_   = nullptr;
    INotifier*  notifier_ = nullptr;

    void process(Order& o) {
        if (logger_)   logger_->info("processando");   // check 1
        // ... lógica ...
        if (logger_)   logger_->info("concluído");     // check 2
        if (notifier_) notifier_->send(o.user, "ok"); // check 3
    }
};
```

```cpp
// Com Null Object — zero checks
class OrderService {
    ILogger&   logger_;    // referência, nunca null
    INotifier& notifier_;  // referência, nunca null

    void process(Order& o) {
        logger_.info("processando");  // sempre seguro
        // ... lógica ...
        logger_.info("concluído");    // sempre seguro
        notifier_.send(o.user, "ok"); // sempre seguro
    }
};
```

---

## Estrutura

```
Interface          Real                  Null
──────────         ──────────────────    ──────────────
ILogger            ConsoleLogger         NullLogger
INotifier          EmailNotifier         NullNotifier
                   SMSNotifier
IUserRepository    InMemoryUserRepo      (via sentinela UNKNOWN_USER)
```

---

## Uso

```cpp
// Em produção — com logger real
ConsoleLogger logger;
PaymentService svc(logger);
svc.charge("alice@example.com", 150.00);

// Em testes — silencioso, sem mudar PaymentService
NullLogger null_logger;
PaymentService svc_test(null_logger);
bool ok = svc_test.charge("bob@example.com", 200.00);
// sem output de log, mas lógica funciona normalmente
```

---

## Null Object vs `std::optional`

| | Null Object | `std::optional<T>` |
|---|---|---|
| **Uso ideal** | Comportamento (interfaces, injeção) | Dados (valores, structs) |
| **Null checks** | Zero — cliente nunca checa | Explícito — `has_value()` |
| **Polimorfismo** | Sim | Não |
| **Segurança** | Caller pode ignorar que é null | Caller é forçado a verificar |

**Regra prática:** `optional` para dados que podem não existir; Null Object para serviços/comportamentos opcionais.

---

## Como compilar

```bash
make build
./dist/bin/null_object
```
