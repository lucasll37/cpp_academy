#pragma once

#include <FGFDMExec.h>
#include <string>

namespace flight {

// ============================================================
// FlightModel
//
// Wrapper RAII em torno do JSBSim FGFDMExec.
//
// Responsabilidades:
//   - Localizar o root dir do JSBSim (pacote .deb instala em
//     /usr/share/JSBSim/ por padrão)
//   - Carregar um modelo de aeronave pelo nome
//   - Avançar a simulação frame a frame (Run)
//   - Expor propriedades de voo por categoria
//
// Propriedades JSBSim usadas:
//   position/h-sl-ft          altitude MSL em pés
//   velocities/vc-kts         airspeed calibrada em nós
//   velocities/vd-fps         velocidade vertical em fps
//   attitude/phi-deg           bank (rolagem) em graus
//   attitude/theta-deg         pitch em graus
//   attitude/psi-deg           heading em graus
//   fcs/elevator-cmd-norm      comando de profundor [-1, 1]
//   fcs/aileron-cmd-norm       comando de aileron  [-1, 1]
//   fcs/rudder-cmd-norm        comando de leme     [-1, 1]
//   fcs/throttle-cmd-norm[0]   manete do motor 0   [ 0, 1]
// ============================================================

struct FlightState {
    // Posição
    double altitude_ft   = 0.0;   // altitude acima do nível do mar (ft)
    double latitude_deg  = 0.0;
    double longitude_deg = 0.0;

    // Velocidades
    double airspeed_kts  = 0.0;   // velocidade calibrada (kts)
    double vspeed_fpm    = 0.0;   // velocidade vertical (ft/min)

    // Atitude
    double pitch_deg     = 0.0;
    double roll_deg      = 0.0;
    double heading_deg   = 0.0;

    // Motor
    double throttle      = 0.0;   // [0, 1]
    double rpm           = 0.0;
};

struct ControlInputs {
    double elevator = 0.0;   // [-1, 1]  + = nariz sobe
    double aileron  = 0.0;   // [-1, 1]  + = asa direita desce
    double rudder   = 0.0;   // [-1, 1]
    double throttle = 0.8;   // [ 0, 1]
};

class FlightModel {
public:
    // Carrega modelo. root_dir = onde ficam aircraft/, engine/, systems/
    // Se root_dir estiver vazio, tenta /usr/share/JSBSim automaticamente.
    explicit FlightModel(const std::string& aircraft_name,
                         const std::string& root_dir = "");

    // Avança a simulação um passo de integração (~1/120 s)
    bool step();

    // Aplica comandos de controle antes do próximo step()
    void set_controls(const ControlInputs& ctrl);

    // Lê o estado atual da aeronave
    FlightState state() const;

    // Acesso direto ao FDM (para propriedades avançadas)
    JSBSim::FGFDMExec& fdm() { return fdm_; }

private:
    JSBSim::FGFDMExec fdm_;
    std::string       aircraft_;
};

} // namespace flight
