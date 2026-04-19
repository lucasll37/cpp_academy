# flight-simulator — Simulação de Dinâmica de Voo com JSBSim

## O que é este projeto?

Integra o motor de dinâmica de voo **JSBSim** com um **autopiloto PID** em C++, simulando o comportamento de uma aeronave (Cessna 172) em voo. O sistema demonstra como modelos físicos de alta fidelidade se integram com sistemas de controle em tempo real.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| **FDM (Flight Dynamics Model)** | Modelo matemático 6-DOF de forças aerodinâmicas |
| **PID Controller** | Controlador proporcional-integral-derivativo para autopiloto |
| **Loop de controle** | Lê estado → calcula controles → aplica → avança simulação |
| **CSV logging** | Registro de dados de voo para análise pós-voo |
| **Wrapper pattern** | Classe que encapsula uma API C complexa (JSBSim) |

---

## Estrutura de arquivos

```
flight-simulator/
├── main.cpp         ← loop principal + missão de voo
├── fdm.hpp/cpp      ← wrapper sobre JSBSim (FlightModel)
├── autopilot.hpp/cpp ← PIDs de altitude, heading, airspeed
├── logger.hpp/cpp   ← CSV writer com taxa configurável
└── deps/
    └── jsbsim/      ← motor de física de voo (submodule)
```

---

## Arquitetura do sistema

```
main.cpp (loop de missão)
    │
    ├── FlightModel (fdm.hpp)    ← encapsula JSBSim
    │       ├── aircraft: c172p (Cessna 172)
    │       ├── FlightState: altitude, speed, pitch, roll, heading
    │       └── ControlInputs: elevator, aileron, rudder, throttle
    │
    ├── Autopilot (autopilot.hpp) ← três PIDs independentes
    │       ├── PID de altitude   → controla elevator
    │       ├── PID de heading    → controla aileron/rudder
    │       └── PID de airspeed   → controla throttle
    │
    └── FlightLogger (logger.hpp) ← grava CSV a cada N frames
```

---

## `FlightState` — estado da aeronave

```cpp
struct FlightState {
    double altitude_ft;  // altitude acima do nível do mar (pés)
    double airspeed_kts; // velocidade indicada (nós)
    double vspeed_fpm;   // velocidade vertical (pés por minuto)
    double pitch_deg;    // arfagem (nariz para cima = positivo)
    double roll_deg;     // rolamento (asa direita baixa = positivo)
    double heading_deg;  // proa (0=Norte, 90=Leste, 180=Sul, 270=Oeste)
    double throttle;     // posição do acelerador (0.0 a 1.0)
    double rpm;          // rotação do motor (RPM)
};
```

---

## `ControlInputs` — entradas de controle

```cpp
struct ControlInputs {
    double elevator;  // profundor  (−1 = morro, +1 = cabrita)
    double aileron;   // aileron    (−1 = rola esquerda, +1 = rola direita)
    double rudder;    // leme       (−1 = esquerda, +1 = direita)
    double throttle;  // acelerador (0.0 = idle, 1.0 = máximo)
};
```

---

## O Autopiloto — PIDs em Cascata

Um **PID** (Proporcional-Integral-Derivativo) calcula o erro entre o valor desejado e o atual e gera um sinal de controle:

```
controle = Kp * erro + Ki * ∫erro dt + Kd * d(erro)/dt
              │             │              │
           reage ao      elimina erro  amorte ce
           erro atual    acumulado     oscilações
```

### Três loops independentes:

```cpp
class Autopilot {
    // Loop 1: altitude → elevator
    // Erro: altitude_desejada − altitude_atual
    // Saída: posição do profundor
    PID alt_pid_{Kp=0.01, Ki=0.001, Kd=0.005};

    // Loop 2: heading → aileron (+ rudder coordenado)
    // Erro: heading_desejado − heading_atual (com wrap de 360°)
    // Saída: posição do aileron
    PID hdg_pid_{Kp=0.02, Ki=0.001, Kd=0.01};

    // Loop 3: airspeed → throttle
    // Erro: speed_desejada − speed_atual
    // Saída: posição do acelerador
    PID spd_pid_{Kp=0.05, Ki=0.01, Kd=0.005};

public:
    ControlInputs update(const FlightState& state, double dt) {
        ControlInputs ctrl;
        ctrl.elevator = alt_pid_.compute(target_alt_ - state.altitude_ft, dt);
        ctrl.aileron  = hdg_pid_.compute(heading_error(target_hdg_, state.heading_deg), dt);
        ctrl.throttle = 0.75 + spd_pid_.compute(target_spd_ - state.airspeed_kts, dt);
        return ctrl;
    }
};
```

---

## Loop de simulação

```cpp
constexpr double DT      = 1.0 / 120.0; // 120 Hz (padrão JSBSim)
constexpr double SIM_END = 240.0;       // 4 minutos de simulação

ap.set_target_altitude_ft(3000.0);
ap.set_target_heading_deg(90.0);
ap.set_target_airspeed_kts(90.0);

while (sim_time <= SIM_END) {
    // Mudanças de missão no meio do voo
    if (sim_time >= 30.0) ap.set_target_altitude_ft(5000.0); // sobe para 5000ft
    if (sim_time >= 90.0) ap.set_target_heading_deg(180.0);  // vira para Sul

    auto state = fdm.state();           // lê o estado atual
    auto ctrl  = ap.update(state, DT); // autopiloto calcula controles
    fdm.set_controls(ctrl);            // aplica controles
    fdm.step();                        // avança a física por DT

    logger.record(sim_time, state);    // grava CSV
    sim_time += DT;
}
```

---

## Missão de voo

```
t=0–30s    Voo nivelado: alt=3000ft, hdg=90°(Leste), spd=90kts
t=30–90s   Subida: autopiloto comanda altitude 5000ft
t=90–180s  Curva: autopiloto comanda heading 180°(Sul)
t=180–240s Velocidade reduzida: spd=80kts, voo nivelado estabilizado
```

---

## Saída esperada

```
  time(s)    alt(ft)  spd(kts)  vs(fpm)  pitch(°)  roll(°)  hdg(°)  thr  rpm
----------------------------------------------------------------------------------
     0.0    3000.0    90.0       +0      0.0       0.0     90.0  0.75  2300
    10.0    3002.1    90.3      +25      0.8       0.1     90.0  0.76  2310
    30.0    3000.8    90.1       +5      0.1       0.0     90.0  0.75  2301
    35.0    3120.4    89.8     +580      6.2       0.2     90.0  0.80  2380  ← subindo
    ...
   240.0    5000.2    80.1       +3      0.1       0.0    180.1  0.73  2280
```

---

## Dados CSV gerados

```
time_s,altitude_ft,airspeed_kts,vspeed_fpm,pitch_deg,roll_deg,heading_deg,throttle,rpm
0.000,3000.00,90.00,0.0,0.0,0.0,90.0,0.750,2300
0.100,3001.20,90.10,144.0,0.8,0.1,90.0,0.762,2310
...
```

---

## Como compilar e executar

```bash
# JSBSim precisa ser inicializado como submodule
git submodule update --init src/flight-simulator/deps/jsbsim

meson setup dist
cd dist && ninja flight_simulator
./bin/flight_simulator
# gera: flight_data.csv
```

---

## Dependências

- `JSBSim` — motor de dinâmica de voo (submodule em `deps/`)
- `spdlog` — logging estruturado
- `fmt` — formatação de saída
