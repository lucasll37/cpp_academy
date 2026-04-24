// =============================================================================
//  singleton/singleton.hpp
//
//  Singleton — instância única garantida
//  ──────────────────────────────────────
//  Garante que uma classe tenha apenas uma instância e fornece ponto de
//  acesso global a ela. Em C++11+, a versão com static local é thread-safe
//  por garantia do padrão (magic statics — §6.7).
//
//  Casos de uso reais: logger, config, registry de plugins, pool de conexões.
//
//  Anti-padrão quando: o estado global dificulta testes ou cria acoplamento
//  oculto. Prefira injeção de dependência quando possível.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <vector>
#include <mutex>

namespace patterns {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Singleton clássico — Meyer's Singleton (C++11, thread-safe)
//
//  O padrão mais simples e correto em C++ moderno.
//  Static local é inicializado na primeira chamada e destruído ao sair do
//  programa — garantido thread-safe pelo compilador.
// ─────────────────────────────────────────────────────────────────────────────
class Logger {
public:
    // instance() → única forma de obter o Logger
    static Logger& instance() {
        static Logger inst; // inicialização thread-safe (C++11 §6.7)
        return inst;
    }

    // Não copiável, não movível — impede criar cópias acidentais
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    void log(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx_);
        entries_.push_back(msg);
        fmt::print("  [Logger] {}\n", msg);
    }

    const std::vector<std::string>& entries() const { return entries_; }
    std::size_t count() const { return entries_.size(); }

private:
    Logger() { fmt::print("  [Logger] construído (só uma vez!)\n"); }
    ~Logger() { fmt::print("  [Logger] destruído\n"); }

    std::vector<std::string> entries_;
    std::mutex               mtx_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  2. Singleton<T> — wrapper genérico sem herança
//
//  Não herda de Singleton<T>: herança forçaria chamar o ctor da base,
//  que está deletado. Em vez disso, T é construído diretamente pela
//  static local — o mesmo mecanismo do Meyer's Singleton acima.
//
//  AppConfig usa o Meyer's Singleton diretamente para ficar mais simples.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
T& singleton_instance() {
    static T inst;
    return inst;
}

struct AppConfig {
    std::string app_name  = "cpp_academy";
    std::string version   = "1.0.0";
    int         log_level = 2;

    static AppConfig& instance() {
        return singleton_instance<AppConfig>();
    }

    // Não copiável
    AppConfig(const AppConfig&)            = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    void print() const {
        fmt::print("  Config: {} v{} (log_level={})\n",
                   app_name, version, log_level);
    }

    AppConfig() = default;
};

} // namespace patterns