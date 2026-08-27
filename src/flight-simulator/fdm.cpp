#include "fdm.hpp"
#include "JSBSim/initialization/FGInitialCondition.h"

// ⚠ `FGFDMExec::GetPropulsion()` devolve um `FGPropulsion*`, e o cabeçalho do
//   FDM só traz a DECLARAÇÃO ADIANTADA dessa classe (`class FGPropulsion;`).
//   Um ponteiro para tipo incompleto pode ser guardado e passado adiante, mas
//   NÃO desreferenciado: chamar `->InitRunning()` exige a definição completa.
//
//     error: invalid use of incomplete type 'class JSBSim::FGPropulsion'
//     note: forward declaration of 'class JSBSim::FGPropulsion'
//
//   É o mesmo mecanismo que faz o pimpl funcionar (src/linkage/, seção 5.3):
//   a biblioteca usa forward declaration justamente para NÃO obrigar todo
//   cliente a incluir a árvore inteira de cabeçalhos. Quem precisa do tipo
//   completo inclui o cabeçalho — e paga por ele só onde precisa.
#include "JSBSim/models/FGPropulsion.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

namespace flight {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// O pacote .deb do JSBSim instala os dados em /usr/share/JSBSim/
// Detectamos automaticamente se o usuário não informou nada.
static std::string resolve_root(const std::string& hint) {
    if (!hint.empty()) return hint;

    // Caminhos candidatos (ordem de preferência)
    const std::vector<fs::path> candidates = {
        "/usr/share/JSBSim",
        "/usr/local/share/JSBSim",
        "/opt/JSBSim",
    };

    for (const auto& p : candidates) {
        if (fs::exists(p / "aircraft")) {
            spdlog::debug("JSBSim root: {}", p.string());
            return p.string();
        }
    }

    throw std::runtime_error(
        "JSBSim root não encontrado. Instale o pacote JSBSim (.deb) "
        "ou passe o caminho explicitamente.");
}

// ─────────────────────────────────────────────────────────────────────────────
// FlightModel
// ─────────────────────────────────────────────────────────────────────────────

FlightModel::FlightModel(const std::string& aircraft_name,
                         const std::string& root_dir)
    : aircraft_(aircraft_name)
{
    const std::string root = fs::absolute(resolve_root(root_dir)).string();

    fdm_.SetRootDir(SGPath(root));
    fdm_.SetDebugLevel(0);

    spdlog::info("Carregando aeronave '{}'...", aircraft_name);

    if (!fdm_.LoadModel(SGPath(root + "/aircraft"),
                        SGPath(root + "/engine"),
                        SGPath(root + "/systems"),
                        aircraft_name)) {
        throw std::runtime_error("Falha ao carregar aeronave: " + aircraft_name);
    }

    auto* ic = fdm_.GetIC();
    ic->SetAltitudeAGLFtIC(3000.0);
    ic->SetVcalibratedKtsIC(90.0);
    ic->SetPsiDegIC(90.0);

    fdm_.RunIC();

    // ⚠ Ligar o motor de verdade.
    //   `propulsion/engine[0]/set-running` só faz `Running = true` em FGEngine;
    //   os magnetos continuam desligados e o motor morre no primeiro
    //   doEngineStartup(), deixando a Cessna virar um planador (rpm = 0).
    //   InitRunning(-1) — "todos os motores" — é o caminho completo: magnetos,
    //   mistura, RPM inicial e GetSteadyState().
    fdm_.GetPropulsion()->InitRunning(-1);

    // ⚠ E MANTER o motor ligado.
    //   `InitRunning` acerta o estado interno do FGEngine, mas quem manda no
    //   motor a cada frame é o FCS: ele empurra `fcs/mixture-cmd-norm[0]` para
    //   dentro do motor em TODO passo. Essa propriedade nasce em 0.0 (mistura
    //   cortada = manete de mistura puxada até o fim, o gesto com que se
    //   DESLIGA um motor a pistão de verdade). Sem a linha abaixo o motor
    //   perde rotação em ~9 s de simulação e a Cessna vira planador.
    fdm_.SetPropertyValue("fcs/mixture-cmd-norm[0]", 1.0);

    fdm_.SetPropertyValue("fcs/throttle-cmd-norm[0]", 0.8);

    spdlog::info("Aeronave '{}' carregada. Simulação pronta.", aircraft_name);
}

bool FlightModel::step() {
    return fdm_.Run();
}

void FlightModel::set_controls(const ControlInputs& ctrl) {
    fdm_.SetPropertyValue("fcs/elevator-cmd-norm",    ctrl.elevator);
    fdm_.SetPropertyValue("fcs/aileron-cmd-norm",     ctrl.aileron);
    fdm_.SetPropertyValue("fcs/rudder-cmd-norm",      ctrl.rudder);
    fdm_.SetPropertyValue("fcs/throttle-cmd-norm[0]", ctrl.throttle);
}

FlightState FlightModel::state() {
    FlightState s;

    s.altitude_ft   = fdm_.GetPropertyValue("position/h-sl-ft");
    s.latitude_deg  = fdm_.GetPropertyValue("position/lat-geod-deg");
    s.longitude_deg = fdm_.GetPropertyValue("position/long-gc-deg");

    s.airspeed_kts  = fdm_.GetPropertyValue("velocities/vc-kts");
    // v-down-fps: eixo Z do referencial local aponta para BAIXO, então descer
    // é positivo. Invertemos o sinal para a convenção de cockpit (subir = +) e
    // multiplicamos por 60 para converter ft/s → ft/min.
    // ⚠ O nome tem que ser exato: "velocities/vd-fps" NÃO existe no JSBSim, e
    //   um nome inexistente devolve 0.0 SILENCIOSAMENTE (era o bug daqui).
    s.vspeed_fpm    = -fdm_.GetPropertyValue("velocities/v-down-fps") * 60.0;

    s.pitch_deg     = fdm_.GetPropertyValue("attitude/theta-deg");
    s.roll_deg      = fdm_.GetPropertyValue("attitude/phi-deg");
    s.heading_deg   = fdm_.GetPropertyValue("attitude/psi-deg");

    s.throttle      = fdm_.GetPropertyValue("fcs/throttle-cmd-norm[0]");
    s.rpm           = fdm_.GetPropertyValue("propulsion/engine[0]/engine-rpm");

    return s;
}

} // namespace flight
