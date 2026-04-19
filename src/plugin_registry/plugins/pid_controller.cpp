#include "../icontroller.hpp"

namespace plugin {

// ─────────────────────────────────────────────────────────────────────────────
// PIDController
//
// Controlador PID clássico aplicado ao pitch e altitude.
// Simples o suficiente para ser didático, mas com estado real (integrador).
//
// O estado interno (integral_, prev_error_) é o motivo pelo qual reset()
// existe na interface: sem ele, episódios consecutivos contaminariam
// uns aos outros através do integrador acumulado.
// ─────────────────────────────────────────────────────────────────────────────

class PIDController : public IController {
public:
    std::string name() const override { return "pid"; }

    void reset() override {
        integral_   = 0.0;
        prev_error_ = 0.0;
    }

    Action compute(const State& s) override {
        // Objetivo simples: manter altitude_ft = 3000 ft, pitch = 0 graus
        constexpr double target_altitude = 3000.0;
        constexpr double dt              = 1.0 / 60.0;  // 60 Hz

        // ── Malha de altitude → elevator ─────────────────────────────────
        //
        // PID clássico: u = Kp*e + Ki*∫e dt + Kd*(de/dt)
        //
        // Ganhos conservadores para uma aeronave genérica:
        constexpr double Kp = 0.002;
        constexpr double Ki = 0.0001;
        constexpr double Kd = 0.005;

        double error    = target_altitude - s.altitude_ft;
        integral_      += error * dt;
        double derivative = (error - prev_error_) / dt;
        prev_error_     = error;

        double elevator = Kp * error + Ki * integral_ + Kd * derivative;

        // Satura em [-1, 1]
        elevator = clamp(elevator, -1.0, 1.0);

        // ── Throttle fixo ─────────────────────────────────────────────────
        // Um PID real teria outra malha para airspeed → throttle.
        // Aqui fixamos para manter o exemplo focado no pitch/altitude.
        constexpr double throttle = 0.75;

        return Action{
            .elevator = elevator,
            .throttle = throttle,
            .aileron  = 0.0,
        };
    }

private:
    double integral_   = 0.0;
    double prev_error_ = 0.0;

    static double clamp(double v, double lo, double hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
};

} // namespace plugin

// ─────────────────────────────────────────────────────────────────────────────
// Ponto de entrada do plugin
//
// Esta única linha expande para as três funções extern "C" que o registry
// vai buscar via dlsym: plugin_name, create_controller, destroy_controller.
//
// Note que PIDController é uma classe C++ normal — vtable, herança, tudo.
// Só o ponto de entrada (as três funções geradas pela macro) precisa ser
// extern "C". O resto do plugin pode ser C++ moderno sem restrição.
// ─────────────────────────────────────────────────────────────────────────────

REGISTER_CONTROLLER(plugin::PIDController, "pid")
