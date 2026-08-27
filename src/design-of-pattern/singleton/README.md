# singleton — Padrão Singleton

## O que é este projeto?

Implementa o padrão **Singleton**: garante que uma classe tenha exatamente uma instância e fornece um ponto de acesso global a ela.

---

## Implementações

### 1. Meyer's Singleton (C++11 — o padrão correto)

```cpp
class Logger {
public:
    static Logger& instance() {
        static Logger inst; // thread-safe por garantia do C++11 §6.7
        return inst;
    }

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default; // construtor privado — ninguém cria diretamente
};

// Uso
Logger::instance().log("mensagem");
```

A variável `static` local é inicializada na primeira chamada e destruída ao sair do programa. O padrão C++11 garante que isso é **thread-safe** sem precisar de mutex manual.

### 2. Singleton\<T\> genérico

```cpp
template <typename T>
class Singleton {
public:
    static T& instance() {
        static T inst;
        return inst;
    }
    Singleton(const Singleton&) = delete;
};

struct AppConfig : Singleton<AppConfig> {
    friend class Singleton<AppConfig>;
    std::string app_name = "cpp_academy";
private:
    AppConfig() = default;
};
```

---

## Quando usar

| Use | Evite |
|---|---|
| Logger global | Estado compartilhado que dificulta testes |
| Config carregada uma vez | Quando injeção de dependência resolve melhor |
| Registry/cache global | Classes que precisam de múltiplas instâncias em testes |
| Pool de conexões | Acoplamento oculto entre módulos |

---

## Anti-padrão vs. uso legítimo

Singleton é frequentemente listado como anti-padrão porque:
- Dificulta testes unitários (estado global persiste entre testes)
- Cria acoplamento oculto (qualquer código pode acessar `Logger::instance()`)
- Viola o princípio da injeção de dependência

**Uso legítimo:** quando realmente existe uma única instância natural no sistema (logger de processo, configuração de aplicação, registry de plugins) e quando testes podem usar um Null Object injetado em vez do singleton.

---

## Como compilar

```bash
make build
./dist/bin/singleton
```
