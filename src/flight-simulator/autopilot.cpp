#include "autopilot.hpp"
#include <algorithm>
#include <cmath>

namespace flight {

// ─────────────────────────────────────────────────────────────────────────────
// PidController
// ─────────────────────────────────────────────────────────────────────────────

PidController::PidController(double kp, double ki, double kd,
                             double output_min, double output_max)
    : kp_(kp), ki_(ki), kd_(kd)
    , out_min_(output_min), out_max_(output_max)
{}

double PidController::update(double error, double dt) {
    if (dt <= 0.0) return 0.0;

    integral_ += error * dt;

    // Anti-windup: clipa o integral para não acumular quando saturado
    double max_integral = (out_max_ - out_min_) / (ki_ > 0.0 ? ki_ : 1.0);
    integral_ = std::clamp(integral_, -max_integral, max_integral);

    double derivative = (error - prev_error_) / dt;
    prev_error_ = error;

    double output = kp_ * error + ki_ * integral_ + kd_ * derivative;
    return std::clamp(output, out_min_, out_max_);
}

void PidController::reset() {
    integral_   = 0.0;
    prev_error_ = 0.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Autopilot
//
// Ganhos empíricos para a Cessna 172 no JSBSim, ajustados na ordem
// "de dentro para fora": primeiro a malha de atitude (pitch/bank),
// depois a malha externa que lhe dá o alvo.
// ─────────────────────────────────────────────────────────────────────────────

Autopilot::Autopilot()
    // Malha EXTERNA de altitude → ângulo de pitch alvo, em GRAUS.
    //   kp=0.02   : 100 ft abaixo do alvo → pede +2° de nariz em cima
    //   ki=0.0005 : elimina o erro residual de algumas dezenas de pés
    //   kd=0.1    : o erro cai quando a aeronave sobe, então a derivada é
    //               -velocidade vertical: freia o pitch alvo ANTES de chegar
    //   limites   : nunca pede mais que +8° (estol) nem menos que -7° (mergulho)
    : alt_pid_(0.02, 0.0005, 0.1, -7.0, 8.0)

    // Malha INTERNA de pitch → elevator
    //   kp=0.05 : 5° de erro de atitude → 0.25 de deflexão
    //   ki=0.01 : segura a atitude contra o momento residual da aeronave
    , pitch_pid_(0.05, 0.01, 0.005, -0.5, 0.5)

    // Malha EXTERNA de heading → ângulo de bank alvo, em GRAUS.
    //   kp=1.0  : 25° de erro de proa → 25° de inclinação (já no limite)
    //   limites : ±25° é a curva padrão da aviação civil (~2 min por volta)
    , hdg_pid_(1.0, 0.0, 0.0, -25.0, 25.0)

    // Malha INTERNA de bank → aileron
    , roll_pid_(0.02, 0.001, 0.005, -0.5, 0.5)

    // Malha de airspeed → throttle (única, sem cascata: o manete age
    // quase diretamente sobre a energia)
    //   kp=0.05 : 10 kts de erro → +0.5 de manete
    , spd_pid_(0.05, 0.005, 0.01, 0.0, 1.0)
{}

void Autopilot::set_target_altitude_ft(double alt)  { target_altitude_ = alt; }
void Autopilot::set_target_heading_deg(double hdg)  { target_heading_  = hdg; }
void Autopilot::set_target_airspeed_kts(double spd) { target_airspeed_ = spd; }

ControlInputs Autopilot::update(const FlightState& state, double dt) {
    ControlInputs ctrl;

    // ── Cascata vertical: altitude → pitch alvo → elevator ────────────────
    const double alt_error = target_altitude_ - state.altitude_ft;
    cmd_pitch_deg_ = alt_pid_.update(alt_error, dt);

    const double pitch_error = cmd_pitch_deg_ - state.pitch_deg;
    // ⚠ Sinal: no JSBSim `fcs/elevator-cmd-norm` POSITIVO abaixa o nariz.
    //   Para SUBIR o nariz (erro de pitch positivo) o comando tem de ser
    //   NEGATIVO. Sem este menos, a realimentação vira positiva e a
    //   aeronave diverge em segundos.
    ctrl.elevator = -pitch_pid_.update(pitch_error, dt);

    // ── Cascata lateral: heading → bank alvo → aileron ────────────────────
    double hdg_error = target_heading_ - state.heading_deg;

    // Wrap para [-180, +180] — sem isto, indo de 350° para 10° o erro cru
    // seria -340° e a aeronave viraria quase uma volta inteira à esquerda.
    while (hdg_error >  180.0) hdg_error -= 360.0;
    while (hdg_error < -180.0) hdg_error += 360.0;

    cmd_bank_deg_ = hdg_pid_.update(hdg_error, dt);

    const double roll_error = cmd_bank_deg_ - state.roll_deg;
    ctrl.aileron = roll_pid_.update(roll_error, dt);

    // Rudder proporcional ao aileron: coordenação básica da curva
    ctrl.rudder = ctrl.aileron * 0.1;

    // ── Airspeed → throttle ───────────────────────────────────────────────
    const double spd_error = target_airspeed_ - state.airspeed_kts;
    ctrl.throttle = spd_pid_.update(spd_error, dt);

    return ctrl;
}

} // namespace flight
