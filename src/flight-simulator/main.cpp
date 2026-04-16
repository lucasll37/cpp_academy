#include "fdm.hpp"
#include "autopilot.hpp"
#include "logger.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <chrono>

// ============================================================
// main.cpp — Simulação de Dinâmica de Voo com JSBSim
//
// Demonstra:
//   1. Inicialização do FGFDMExec via wrapper FlightModel
//   2. Loop de simulação a 120 Hz (passo padrão do JSBSim)
//   3. Autopiloto com PIDs de altitude, heading e airspeed
//   4. Log de dados de voo em CSV
//
// Cenário:
//   t=0–30s   : voo nivelado, autopiloto mantém 3000ft / 90kts / hdg=90
//   t=30–90s  : altitude climb para 5000ft
//   t=90–180s : heading change para 180° (virada sul)
//   t=180s+   : voo nivelado no novo heading até o fim
//
// Saída:
//   - Console: estado a cada 10s de simulação
//   - flight_data.csv: dados a cada 100ms (~12 frames)
// ============================================================

static void print_state(double t, const flight::FlightState& s) {
    fmt::println(
        "[{:6.1f}s] alt={:7.1f}ft  spd={:5.1f}kts  vs={:+6.0f}fpm  "
        "pitch={:+5.1f}°  roll={:+5.1f}°  hdg={:5.1f}°  thr={:.2f}  rpm={:.0f}",
        t,
        s.altitude_ft, s.airspeed_kts, s.vspeed_fpm,
        s.pitch_deg, s.roll_deg, s.heading_deg,
        s.throttle, s.rpm);
}

int main() {
    spdlog::set_level(spdlog::level::info);

    // ── Inicialização ─────────────────────────────────────────────────────
    fmt::println("=== JSBSim Flight Dynamics Demo ===\n");

    flight::FlightModel   fdm("c172p");
    flight::Autopilot     ap;
    flight::FlightLogger  logger("flight_data.csv", /*every_n=*/12);

    ap.set_target_altitude_ft(3000.0);
    ap.set_target_heading_deg(90.0);
    ap.set_target_airspeed_kts(90.0);

    // JSBSim roda a 120 Hz por padrão (dt = 1/120 s ≈ 8.33ms)
    constexpr double DT        = 1.0 / 120.0;
    constexpr double SIM_END   = 240.0;  // segundos de simulação
    constexpr int    PRINT_HZ  = 10;     // print a cada 10s

    double sim_time    = 0.0;
    int    print_frame = static_cast<int>(PRINT_HZ / DT);
    int    frame       = 0;

    fmt::println("{:>8}  {:>9}  {:>9}  {:>9}  {:>8}  {:>8}  {:>8}  {:>5}  {:>6}",
        "time(s)", "alt(ft)", "spd(kts)", "vs(fpm)",
        "pitch(°)", "roll(°)", "hdg(°)", "thr", "rpm");
    fmt::println("{}", std::string(90, '-'));

    auto wall_start = std::chrono::steady_clock::now();

    // ── Loop principal ────────────────────────────────────────────────────
    while (sim_time <= SIM_END) {

        // Mudança de missão no meio do voo
        if (sim_time >= 30.0  && sim_time < 30.0 + DT)
            ap.set_target_altitude_ft(5000.0);

        if (sim_time >= 90.0  && sim_time < 90.0 + DT)
            ap.set_target_heading_deg(180.0);

        if (sim_time >= 180.0 && sim_time < 180.0 + DT)
            ap.set_target_airspeed_kts(80.0);  // reduz velocidade de cruzeiro

        // Calcula inputs do autopiloto
        auto state = fdm.state();
        auto ctrl  = ap.update(state, DT);
        fdm.set_controls(ctrl);

        // Avança simulação
        if (!fdm.step()) break;

        // Log CSV
        logger.record(sim_time, state);

        // Print console
        if (frame % print_frame == 0) {
            print_state(sim_time, state);
        }

        sim_time += DT;
        ++frame;
    }

    // ── Resumo ────────────────────────────────────────────────────────────
    auto wall_end = std::chrono::steady_clock::now();
    double wall_s = std::chrono::duration<double>(wall_end - wall_start).count();

    fmt::println("{}", std::string(90, '-'));
    fmt::println("\nSimulação concluída.");
    fmt::println("  Tempo simulado  : {:.1f} s", sim_time);
    fmt::println("  Tempo real      : {:.2f} s  ({:.0f}x realtime)",
                 wall_s, sim_time / wall_s);
    fmt::println("  Registros CSV   : {}", logger.records_written());
    fmt::println("  Arquivo gerado  : flight_data.csv");

    return 0;
}
