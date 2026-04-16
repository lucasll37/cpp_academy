#include "fdm.hpp"

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
    const std::string root = resolve_root(root_dir);

    fdm_.SetRootDir(SGPath(root));
    fdm_.SetAircraftPath(SGPath(root + "/aircraft"));
    fdm_.SetEnginePath(SGPath(root + "/engine"));
    fdm_.SetSystemsPath(SGPath(root + "/systems"));

    // Silencia o output padrão do JSBSim para não poluir o terminal
    fdm_.SetDebugLevel(0);

    spdlog::info("Carregando aeronave '{}'...", aircraft_name);

    if (!fdm_.LoadModel(aircraft_name)) {
        throw std::runtime_error("Falha ao carregar aeronave: " + aircraft_name);
    }

    // Condições iniciais: voo nivelado a 3000 ft, 90 kts
    auto* ic = fdm_.GetIC();
    ic->SetAltitudeAGLFtIC(3000.0);
    ic->SetVcalibratedKtsIC(90.0);
    ic->SetPsiDegIC(90.0);   // heading leste

    fdm_.RunIC();

    // Motor ligado (throttle inicial em 80%)
    fdm_.SetPropertyValue("fcs/throttle-cmd-norm[0]", 0.8);
    fdm_.SetPropertyValue("propulsion/engine[0]/set-running", 1.0);

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

FlightState FlightModel::state() const {
    FlightState s;

    s.altitude_ft   = fdm_.GetPropertyValue("position/h-sl-ft");
    s.latitude_deg  = fdm_.GetPropertyValue("position/lat-geod-deg");
    s.longitude_deg = fdm_.GetPropertyValue("position/long-gc-deg");

    s.airspeed_kts  = fdm_.GetPropertyValue("velocities/vc-kts");
    // vd-fps = velocidade descendente positiva; invertemos para fpm convencional
    s.vspeed_fpm    = -fdm_.GetPropertyValue("velocities/vd-fps") * 60.0;

    s.pitch_deg     = fdm_.GetPropertyValue("attitude/theta-deg");
    s.roll_deg      = fdm_.GetPropertyValue("attitude/phi-deg");
    s.heading_deg   = fdm_.GetPropertyValue("attitude/psi-deg");

    s.throttle      = fdm_.GetPropertyValue("fcs/throttle-cmd-norm[0]");
    s.rpm           = fdm_.GetPropertyValue("propulsion/engine[0]/engine-rpm");

    return s;
}

} // namespace flight
