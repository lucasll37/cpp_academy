#pragma once

#include "fdm.hpp"
#include <string>
#include <fstream>

namespace flight {

// ============================================================
// FlightLogger
//
// Grava o estado da aeronave em CSV a cada N frames.
// Formato:
//   time_s, altitude_ft, airspeed_kts, vspeed_fpm,
//   pitch_deg, roll_deg, heading_deg, throttle, rpm
//
// Uso:
//   FlightLogger log("flight_data.csv", /*every_n_frames=*/12);
//   while (sim.step()) {
//       log.record(sim_time, sim.state());
//   }
//   // Arquivo fechado automaticamente no destrutor
// ============================================================
class FlightLogger {
public:
    explicit FlightLogger(const std::string& filepath,
                          int every_n_frames = 12);
    ~FlightLogger();

    void record(double sim_time_s, const FlightState& state);

    int records_written() const { return records_; }

private:
    std::ofstream file_;
    int           every_n_;
    int           frame_count_ = 0;
    int           records_     = 0;
};

} // namespace flight
