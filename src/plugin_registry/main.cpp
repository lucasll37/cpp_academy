#include "plugin_registry.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <iostream>
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Simulação fictícia
//
// Em vez de conectar ao JSBSim aqui (o subprojeto é didático e isolado),
// simulamos um estado que deriva linearmente ao longo do tempo.
// O objetivo é demonstrar o registry, não a física da aeronave.
// ─────────────────────────────────────────────────────────────────────────────

plugin::State simulate_step(const plugin::State& s, const plugin::Action& a) {
    plugin::State next = s;

    // Modelo extremamente simplificado:
    // - elevator positivo → sobe (pitch up → ganha altitude)
    // - throttle → aumenta speed
    // - sem atmosfera, sem aerodinâmica real
    next.pitch_deg   += a.elevator * 2.0;
    next.altitude_ft += std::sin(next.pitch_deg * M_PI / 180.0) * 10.0;
    next.speed_kts   += (a.throttle - 0.75) * 0.5;

    return next;
}

void print_state(const plugin::State& s, const plugin::Action& a) {
    fmt::println("  alt={:7.1f} ft  spd={:5.1f} kts  pitch={:5.1f}°  "
                 "| elev={:+.3f}  thr={:.3f}",
                 s.altitude_ft, s.speed_kts, s.pitch_deg,
                 a.elevator, a.throttle);
}

// ─────────────────────────────────────────────────────────────────────────────
// Roda N steps com o controlador informado
// ─────────────────────────────────────────────────────────────────────────────

void run_episode(const std::string& ctrl_name, int steps = 10) {
    auto& reg = plugin::ControllerRegistry::instance();

    // unique_ptr com deleter customizado: o tipo completo inclui o deleter.
    // Isso é necessário porque o deleter não é o operator delete padrão —
    // é a destroy_controller do .so, capturada no momento do create().
    auto ctrl = reg.create(ctrl_name);

    fmt::println("\n=== Controlador: {} ===", ctrl->name());

    // Estado inicial: aeronave desviada da referência para ver a correção.
    plugin::State state{
        .altitude_ft = 2800.0,   // 200 ft abaixo da referência
        .speed_kts   = 85.0,     // 5 kts abaixo
        .pitch_deg   = -2.0,     // nariz levemente para baixo
    };

    ctrl->reset();

    for (int i = 0; i < steps; ++i) {
        plugin::Action action = ctrl->compute(state);
        fmt::print("  step {:02d} ", i + 1);
        print_state(state, action);
        state = simulate_step(state, action);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    fmt::println("=== Plugin Registry Demo ===\n");

    // Diretório de plugins: primeiro argumento ou padrão.
    std::string plugin_dir = (argc > 1) ? argv[1] : "plugins";
    // std::string plugin_dir = (argc > 1) ? argv[1] : binary_dir() + "/../plugins";

    auto& reg = plugin::ControllerRegistry::instance();

    // ── 1. Descoberta automática ──────────────────────────────────────────
    fmt::println("Varrendo '{}'...", plugin_dir);
    int n = reg.load_directory(plugin_dir);
    fmt::println("{} plugin(s) carregado(s).\n", n);

    // ── 2. Lista plugins disponíveis ─────────────────────────────────────
    fmt::println("Plugins disponíveis:");
    for (const auto& name : reg.available()) {
        fmt::println("  - {}", name);
    }

    // ── 3. Roda um episódio com cada controlador ──────────────────────────
    for (const auto& name : reg.available()) {
        run_episode(name);
    }

    // ── 4. Demonstra troca em runtime ─────────────────────────────────────
    fmt::println("\n=== Troca de controlador em runtime ===");
    fmt::println("(mesma interface, implementação diferente, zero recompilação)\n");

    std::vector<std::string> sequence = {"pid", "lqr", "pid"};
    plugin::State state{ .altitude_ft = 2900.0, .speed_kts = 88.0 };

    for (const auto& name : sequence) {
        if (!reg.has(name)) {
            fmt::println("  [aviso] '{}' não disponível, pulando.", name);
            continue;
        }
        auto ctrl = reg.create(name);
        ctrl->reset();
        plugin::Action action = ctrl->compute(state);
        fmt::print("  [{}] ", name);
        print_state(state, action);
        state = simulate_step(state, action);
    }

    fmt::println("\nFim.");
    return 0;
}
