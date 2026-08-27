#pragma once

#include "fdm.hpp"

namespace flight {

// ============================================================
// PidController
//
// Controlador PID genérico com saturação de saída e anti-windup.
// O autopiloto instancia cinco: altitude, pitch, heading, bank e
// airspeed. Os dois últimos argumentos do construtor são os limites
// de saída — a saída é sempre clampada dentro deles.
// ============================================================
class PidController {
public:
    PidController(double kp, double ki, double kd,
                  double output_min, double output_max);

    // Calcula o output dado o erro e dt (segundos)
    double update(double error, double dt);

    void reset();

private:
    double kp_, ki_, kd_;
    double out_min_, out_max_;
    double integral_  = 0.0;
    double prev_error_ = 0.0;
};

// ============================================================
// Autopilot
//
// Cinco PIDs organizados em TRÊS malhas — duas delas em CASCATA.
//
//   altitude (malha externa)  →  ângulo de pitch alvo [-7°, +8°]
//   pitch    (malha interna)  →  elevator [-0.5, +0.5]
//
//   heading  (malha externa)  →  ângulo de bank alvo [-25°, +25°]
//   bank     (malha interna)  →  aileron [-0.5, +0.5]
//     (rudder acoplado ao aileron para coordenação básica)
//
//   airspeed (malha única)    →  throttle [0, 1]
//
// Por que cascata: o elevator não comanda altitude, comanda MOMENTO de
// arfagem. Ligar altitude direto no elevator ignora essa distância e o
// resultado é oscilação de fugóide — a aeronave sobe, estola e cai.
// A malha externa decide "que atitude eu quero"; a interna cuida de
// manter a atitude, que é o que a superfície de controle realmente faz.
// ============================================================
class Autopilot {
public:
    Autopilot();

    // Define alvos
    void set_target_altitude_ft(double alt);
    void set_target_heading_deg(double hdg);
    void set_target_airspeed_kts(double spd);

    // Calcula e retorna os inputs de controle dado o estado atual
    ControlInputs update(const FlightState& state, double dt);

    // Saídas das malhas externas — úteis para depurar/plotar a cascata
    double commanded_pitch_deg() const { return cmd_pitch_deg_; }
    double commanded_bank_deg()  const { return cmd_bank_deg_;  }

private:
    double target_altitude_  = 3000.0;
    double target_heading_   = 90.0;
    double target_airspeed_  = 90.0;

    PidController alt_pid_;    // altitude → pitch alvo (graus)
    PidController pitch_pid_;  // pitch    → elevator
    PidController hdg_pid_;    // heading  → bank alvo (graus)
    PidController roll_pid_;   // bank     → aileron
    PidController spd_pid_;    // airspeed → throttle

    double cmd_pitch_deg_ = 0.0;
    double cmd_bank_deg_  = 0.0;
};

} // namespace flight
