#include "singleton.hpp"
#include <fmt/core.h>
#include <thread>

int main() {
    fmt::print("── Singleton: Meyer's Singleton ──\n\n");

    // Primeira chamada cria a instância
    auto& log1 = patterns::Logger::instance();
    auto& log2 = patterns::Logger::instance();

    fmt::print("  &log1 == &log2: {} (mesma instância)\n\n",
               &log1 == &log2);

    log1.log("sistema iniciado");
    log2.log("usuário conectou"); // log1 e log2 são o mesmo objeto

    fmt::print("  Total de entradas: {}\n\n", log1.count());

    fmt::print("── Singleton<T> genérico ──\n\n");
    auto& cfg = patterns::AppConfig::instance();
    cfg.app_name = "meu_app";
    cfg.print();

    // Acesso de outro "módulo" — mesmo objeto
    patterns::AppConfig::instance().log_level = 3;
    cfg.print();

    fmt::print("\n── Singleton com threads ──\n\n");
    // Múltiplas threads usando o mesmo logger
    std::thread t1([]{
        patterns::Logger::instance().log("thread 1: evento A");
    });
    std::thread t2([]{
        patterns::Logger::instance().log("thread 2: evento B");
    });
    t1.join(); t2.join();

    fmt::print("\n  Total final: {} entradas\n", patterns::Logger::instance().count());
}
