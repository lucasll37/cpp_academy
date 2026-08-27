#include "../icontroller.hpp"
#include <cmath>

namespace plugin {

// ─────────────────────────────────────────────────────────────────────────────
// LQRController
//
// Linear Quadratic Regulator simplificado (sem Eigen para manter o plugin
// independente de dependências externas).
//
// LQR real resolve a equação de Riccati para encontrar K ótimo dado Q e R.
// Aqui usamos K pré-calculado para um modelo linearizado do C172 em cruzeiro:
//   altitude = 3000 ft, speed = 90 kts
//
// A lei de controle é: u = -K * (x - x_ref)
// onde x = [altitude_error, pitch_deg, speed_error] e u = [elevator, throttle]
//
// Este plugin demonstra que cada .so é completamente isolado:
// tem seu próprio estado, seus próprios ganhos, e não interfere no PID.
// ─────────────────────────────────────────────────────────────────────────────

class LQRController : public IController {
public:
    std::string name() const override { return "lqr"; }

    void reset() override {
        // LQR estático não tem estado acumulado — o reset é trivial.
        // Se fosse um LQR com estimador de estado (Kalman), aqui
        // resetaríamos a covariância P e o estado estimado x_hat.
    }

    Action compute(const State& s) override {
        // ── Vetor de erro de estado ───────────────────────────────────────
        //
        // x = [e_altitude, e_pitch, e_speed]
        // onde e_X = X_ref - X_atual
        constexpr double ref_altitude = 3000.0;  // ft
        constexpr double ref_pitch    = 0.0;     // graus (voo nivelado)
        constexpr double ref_speed    = 90.0;    // kts

        double e_alt   = ref_altitude - s.altitude_ft;
        double e_pitch = ref_pitch    - s.pitch_deg;
        double e_speed = ref_speed    - s.speed_kts;

        // ── Ganhos K pré-calculados ───────────────────────────────────────
        //
        // Para um LQR real, K = R^-1 * B^T * P onde P resolve a DARE.
        // Aqui são valores heurísticos que ilustram a estrutura:
        //
        //   elevator = K_alt * e_alt + K_pitch * e_pitch
        //   throttle = K_speed * e_speed
        //
        constexpr double K_alt   =  0.0015;
        constexpr double K_pitch =  0.08;
        constexpr double K_speed = -0.02;   // negativo: mais speed → menos throttle

        double elevator = K_alt * e_alt + K_pitch * e_pitch;
        double throttle = 0.75 + K_speed * e_speed;

        // Saturação
        elevator = clamp(elevator, -1.0,  1.0);
        throttle = clamp(throttle,  0.0,  1.0);

        return Action{
            .elevator = elevator,
            .throttle = throttle,
            .aileron  = 0.0,
        };
    }

private:
    static double clamp(double v, double lo, double hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
};

} // namespace plugin

REGISTER_CONTROLLER(plugin::LQRController, "lqr")
