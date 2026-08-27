#include "logger.hpp"
#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace flight {

FlightLogger::FlightLogger(const std::string& filepath, int every_n_frames)
    : file_(filepath), every_n_(every_n_frames)
{
    if (!file_.is_open()) {
        throw std::runtime_error("Não foi possível abrir: " + filepath);
    }

    // Cabeçalho CSV
    file_ << "time_s,altitude_ft,airspeed_kts,vspeed_fpm,"
             "pitch_deg,roll_deg,heading_deg,throttle,rpm\n";

    spdlog::info("FlightLogger: gravando em '{}' (a cada {} frames)",
                 filepath, every_n_frames);
}

FlightLogger::~FlightLogger() {
    if (file_.is_open()) {
        spdlog::info("FlightLogger: {} registros gravados.", records_);
        file_.close();
    }
}

void FlightLogger::record(double sim_time_s, const FlightState& state) {
    ++frame_count_;
    if (frame_count_ % every_n_ != 0) return;

    file_ << fmt::format("{:.2f},{:.1f},{:.1f},{:.0f},{:.2f},{:.2f},{:.1f},{:.3f},{:.0f}\n",
        sim_time_s,
        state.altitude_ft,
        state.airspeed_kts,
        state.vspeed_fpm,
        state.pitch_deg,
        state.roll_deg,
        state.heading_deg,
        state.throttle,
        state.rpm);

    ++records_;
}

} // namespace flight
