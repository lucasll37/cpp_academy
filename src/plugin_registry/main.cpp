#include "plugin_registry.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <system_error>

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

// ─────────────────────────────────────────────────────────────────────────────
// Onde estão os plugins?
//
// Um programa que carrega plugins não pode depender do diretório de trabalho:
// o mesmo binário roda de `dist/bin/` (plugins em `dist/plugins/`) e de dentro
// do build dir (plugins em `<exe>/plugins/`). Resolver relativo ao CWD faz o
// programa funcionar ou não dependendo de onde foi invocado.
//
// A solução portável no Linux é `/proc/self/exe`: um symlink para o próprio
// executável, do qual se extrai o diretório real (já resolvendo symlinks).
// ─────────────────────────────────────────────────────────────────────────────
static std::filesystem::path diretorio_do_executavel() {
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path{} : exe.parent_path();
}

static std::string localizar_plugins(int argc, char* argv[],
                                     std::vector<std::string>& tentados) {
    // 1. Argumento explícito tem prioridade absoluta.
    if (argc > 1) return argv[1];

    const auto exe_dir = diretorio_do_executavel();
    std::vector<std::filesystem::path> candidatos;

    if (!exe_dir.empty()) {
        candidatos.push_back(exe_dir / ".." / "plugins");  // layout instalado: dist/bin → dist/plugins
        candidatos.push_back(exe_dir / "plugins");         // layout de build: ao lado do exe
    }
    candidatos.emplace_back("plugins");                    // último recurso: o CWD

    for (const auto& c : candidatos) {
        std::error_code ec;
        if (std::filesystem::is_directory(c, ec)) {
            return std::filesystem::weakly_canonical(c, ec).string();
        }
        tentados.push_back(c.string());
    }
    return {};
}

int main(int argc, char* argv[]) {
    fmt::println("=== Plugin Registry Demo ===\n");

    std::vector<std::string> tentados;
    const std::string plugin_dir = localizar_plugins(argc, argv, tentados);

    if (plugin_dir.empty()) {
        fmt::println("Nenhum diretório de plugins encontrado. Tentei:");
        for (const auto& t : tentados) fmt::println("  - {}", t);
        fmt::println("\nPasse o caminho explicitamente:");
        fmt::println("  {} <dir-com-os-.so>", argv[0]);
        return 1;                       // falha clara, em vez de exceção não tratada
    }

    auto& reg = plugin::ControllerRegistry::instance();

    // ── 1. Descoberta automática ──────────────────────────────────────────
    //
    // load_directory LANÇA se o diretório não puder ser aberto (é o caso de um
    // caminho errado passado em argv[1]). Sem este try, a exceção escaparia do
    // main e o programa terminaria em std::terminate — um abort feio, sem
    // mensagem útil. Um erro previsível do usuário merece saída limpa.
    fmt::println("Varrendo '{}'...", plugin_dir);
    int n = 0;
    try {
        n = reg.load_directory(plugin_dir);
    } catch (const std::exception& e) {
        fmt::println("erro: {}", e.what());
        return 1;
    }
    fmt::println("{} plugin(s) carregado(s).\n", n);

    if (n == 0) {
        fmt::println("O diretório existe mas não contém nenhum .so carregável.");
        return 1;
    }

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
