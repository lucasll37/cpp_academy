#pragma once

#include "fdm.hpp"

namespace flight {

// ============================================================
// PidController
//
// Controlador PID genérico com anti-windup por saturação.
// Usado no autopiloto para altitude e heading.
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
// Dois loops de controle independentes:
//
//   altitude_hold:
//     Erro = altitude_target - altitude_atual
//     Saída → elevator [-1, 1]
//     Estratégia: pitch para subir/descer, mantendo airspeed
//
//   heading_hold:
//     Erro = heading_target - heading_atual (com wrap ±180°)
//     Saída → aileron [-1, 1]
//     (rudder acoplado via relação fixa para coordenação)
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

private:
    double target_altitude_  = 3000.0;
    double target_heading_   = 90.0;
    double target_airspeed_  = 90.0;

    PidController alt_pid_;  // controla elevator
    PidController hdg_pid_;  // controla aileron
    PidController spd_pid_;  // controla throttle
};

} // namespace flight
