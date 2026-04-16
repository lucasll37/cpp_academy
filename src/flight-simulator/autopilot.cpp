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
// Ganhos empíricos para a Cessna 172 no JSBSim.
// Altitude: resposta lenta (aeronave de GA, pitch suave)
// Heading:  resposta mais rápida via aileron
// Speed:    throttle proporcional ao erro de velocidade
// ─────────────────────────────────────────────────────────────────────────────

Autopilot::Autopilot()
    // PID altitude → elevator
    //   kp=0.001: 100ft de erro → 0.1 de elevator (pitch ~2°)
    //   ki=0.0001: integra lentamente
    //   kd=0.005: amortece variações rápidas de altitude
    : alt_pid_(0.001, 0.0001, 0.005, -0.3, 0.3)

    // PID heading → aileron
    //   kp=0.02: 10° de erro → 0.2 de aileron (bank ~12°)
    //   kd=0.01: amortece overshoots de heading
    , hdg_pid_(0.02, 0.0, 0.01, -0.5, 0.5)

    // PID airspeed → throttle
    //   kp=0.05: 10 kts de erro → +0.5 de throttle
    , spd_pid_(0.05, 0.005, 0.01, 0.0, 1.0)
{}

void Autopilot::set_target_altitude_ft(double alt)  { target_altitude_ = alt; }
void Autopilot::set_target_heading_deg(double hdg)  { target_heading_  = hdg; }
void Autopilot::set_target_airspeed_kts(double spd) { target_airspeed_ = spd; }

ControlInputs Autopilot::update(const FlightState& state, double dt) {
    ControlInputs ctrl;

    // ── Altitude hold → elevator ──────────────────────────────────────────
    double alt_error = target_altitude_ - state.altitude_ft;
    ctrl.elevator = -alt_pid_.update(alt_error, dt);
    // Sinal negativo: elevator positivo (nariz baixo) quando altitude > alvo

    // ── Heading hold → aileron ────────────────────────────────────────────
    double hdg_error = target_heading_ - state.heading_deg;

    // Wrap para [-180, +180] — evita virar 350° na direção errada
    while (hdg_error >  180.0) hdg_error -= 360.0;
    while (hdg_error < -180.0) hdg_error += 360.0;

    ctrl.aileron = hdg_pid_.update(hdg_error, dt);

    // Rudder proporcional ao aileron para coordenação básica
    ctrl.rudder = ctrl.aileron * 0.1;

    // ── Airspeed hold → throttle ──────────────────────────────────────────
    double spd_error = target_airspeed_ - state.airspeed_kts;
    ctrl.throttle = spd_pid_.update(spd_error, dt);

    return ctrl;
}

} // namespace flight
