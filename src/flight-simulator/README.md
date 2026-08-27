# flight-simulator — JSBSim e um autopiloto PID escrito do zero

## O que é este projeto?

Uma Cessna 172 voando de verdade: a física vem do **JSBSim** — o mesmo motor de dinâmica de voo
usado pelo FlightGear — e o piloto é um **autopiloto em cascata** de cinco PIDs escrito neste
subprojeto. A missão dura 240 s simulados: nivelar a 3000 ft, subir para 5000 ft, virar de leste
para sul e reduzir a velocidade de cruzeiro. Tudo é gravado num CSV para você plotar depois.

```bash
# 1) dependências do JSBSim (uma vez só): pacote Conan + dados da aeronave
cd src/flight-simulator/deps && ./install.sh && cd -

# 2) configurar e compilar o repositório
make configure
make build

# 3) rodar — A PARTIR DA RAIZ DO REPOSITÓRIO
./dist/bin/flight
```

> ⚠ **Rode a partir da raiz.** `main.cpp` passa o caminho **relativo**
> `"src/flight-simulator/deps/jsbsim/data"` ao `FlightModel`. De qualquer outro diretório o
> programa morre com `Falha ao carregar aeronave: c172p`.

> ⚠ **O alvo se chama `flight`**, não `flight_simulator`:
> `meson compile -C build flight` compila só ele; o binário instalado é `./dist/bin/flight`.

> ⚠ **Não existe submodule.** Este repositório não tem `.gitmodules`; `git submodule update`
> não faz nada aqui. Quem traz o JSBSim é `src/flight-simulator/deps/install.sh`, que roda
> `conan create ./jsbsim` (Debug e Release) e clona os dados com
> `git clone --depth=1 https://github.com/JSBSim-Team/jsbsim ./jsbsim/data`.

---

## Antes de começar: os conceitos, do zero

Nada aqui pressupõe conhecimento de aviação nem de teoria de controle. Leia esta seção inteira
antes do código — todo termo usado adiante é definido aqui.

### 1. O que é um FDM

**FDM** = *Flight Dynamics Model*, modelo de dinâmica de voo. É o programa que responde a uma
única pergunta, milhares de vezes por segundo:

> dadas a atitude, a velocidade e a posição atuais da aeronave, mais a posição das superfícies de
> controle e do motor, **onde e como ela estará daqui a 1/120 de segundo?**

Ele faz isso somando as quatro forças clássicas e integrando o resultado:

```
                      SUSTENTAÇÃO (lift)
                            ▲
                            │
        ARRASTO   ◄──────┼──┼──────►   EMPUXO (thrust)
        (drag)         ╱─────╲          do motor/hélice
                      ╱  ✈    ╲
                            │
                            ▼
                         PESO (weight)

  sobe se sustentação > peso · cos(inclinação);  acelera se empuxo > arrasto
```

A sustentação depende do **ângulo de ataque** e do quadrado da velocidade, com coeficientes
tabelados por aeronave. É por isso que o JSBSim precisa de **arquivos XML descrevendo a
aeronave**: geometria, massa, tabelas de coeficientes aerodinâmicos, curvas do motor, trem de
pouso. O `LoadModel` recebe três diretórios separados:

| Diretório | Contém |
|---|---|
| `data/aircraft/` | `c172p/c172p.xml` — aerodinâmica, massas, sistemas da célula |
| `data/engine/` | motor e hélice (`eng_io320.xml`, `prop_75in2f.xml`, …) |
| `data/systems/` | sistemas reutilizáveis entre aeronaves (piloto automático, FCS genérico) |

### 2. 6-DOF: os seis graus de liberdade

Um corpo rígido no espaço tem **seis** maneiras independentes de se mover — três de translação e
três de rotação. É o que "6-DOF" (*six degrees of freedom*) quer dizer:

```
            ▲ Z (para cima)          Rotações, com os nomes gregos que o JSBSim usa:
            │                          φ (phi)   ROLL   — em torno de X (asa sobe/desce)
            │                          θ (theta) PITCH  — em torno de Y (nariz sobe/desce)
            └──────► Y (asa direita)   ψ (psi)   YAW    — em torno de Z (nariz esquerda/direita)
           ╱
          ▼ X (nariz)                Translações: avanço, deriva lateral, subida/descida
```

A ponte de vocabulário que este README usa o tempo todo:

| Grego (JSBSim) | Propriedade | Português | Superfície que o comanda |
|---|---|---|---|
| φ phi | `attitude/phi-deg` | rolagem / *bank* | aileron |
| θ theta | `attitude/theta-deg` | arfagem / *pitch* | profundor (*elevator*) |
| ψ psi | `attitude/psi-deg` | proa / *heading* | leme (*rudder*) — e, na prática, a curva inclinada |

> Uma aeronave **não** vira com o leme: ela **inclina** (rola) e a componente horizontal da
> sustentação faz a curva. O leme só mantém o nariz alinhado com a trajetória. Isso explica a
> cascata `heading → bank → aileron` que o autopiloto usa.

### 3. A árvore de propriedades do JSBSim

Este é **o** conceito central da biblioteca. Todo o estado da simulação vive numa árvore de nós
endereçados por **string**, como um sistema de arquivos:

```
/
├── position/
│   ├── h-sl-ft            altitude acima do nível do mar, em pés
│   ├── lat-geod-deg       latitude geodésica
│   └── long-gc-deg        longitude geocêntrica
├── velocities/
│   ├── vc-kts             velocidade CALIBRADA, em nós
│   └── v-down-fps         componente vertical, positiva para BAIXO, em ft/s
├── attitude/              phi-deg, theta-deg, psi-deg
├── fcs/                   elevator-cmd-norm, aileron-cmd-norm, rudder-cmd-norm,
│                          throttle-cmd-norm[0], mixture-cmd-norm[0]
└── propulsion/engine[0]/  engine-rpm, set-running, …
```

Ler e escrever é sempre a mesma dupla de funções:

```cpp
double alt = fdm_.GetPropertyValue("position/h-sl-ft");        // ler um sensor
fdm_.SetPropertyValue("fcs/elevator-cmd-norm", -0.12);         // mover um controle
```

Os sufixos dizem a **unidade** — e ler o sufixo errado é um erro silencioso de fator 60 ou de
1,69:

| Sufixo | Unidade | Exemplo |
|---|---|---|
| `-ft` | pés | `position/h-sl-ft` |
| `-kts` | nós (milhas náuticas por hora) | `velocities/vc-kts` |
| `-fps` | pés por segundo | `velocities/v-down-fps` |
| `-deg` | graus | `attitude/theta-deg` |
| `-norm` | normalizado, [-1, 1] ou [0, 1] | `fcs/elevator-cmd-norm` |
| `-cmd-` | **comando** (o que você pede), não a posição efetiva | `fcs/throttle-cmd-norm[0]` |

> ⚠ **Um nome errado NÃO dá erro.** A árvore é criada sob demanda: escrever em
> `"fcs/elevador-cmd-norm"` cria um nó novo, que ninguém lê, e o seu comando some em silêncio.
> Ler de um nome inexistente devolve `0.0`. Este subprojeto já teve exatamente esse bug — o
> comentário em `fdm.cpp` o registra: `"velocities/vd-fps"` não existe, e a velocidade vertical
> aparecia sempre como zero. O nome certo é `velocities/v-down-fps`.

### 4. Airspeed calibrada, MSL e AGL

Três distinções que confundem quem chega agora:

- **Velocidade calibrada (`vc-kts`)** é a que o instrumento do painel mostra, derivada da pressão
  dinâmica. Ela **não** é a velocidade em relação ao solo nem a velocidade verdadeira no ar: com
  o ar rarefeito da altitude, a mesma calibrada corresponde a uma velocidade real maior. Controlar
  a calibrada é o que um piloto faz, porque é ela que determina se a asa ainda sustenta.
- **MSL** (*mean sea level*) — altitude acima do nível do mar. É `position/h-sl-ft`, o que o
  autopiloto lê.
- **AGL** (*above ground level*) — altura acima do terreno logo abaixo. É o que a condição inicial
  usa: `SetAltitudeAGLFtIC(3000)`.

Como o cenário começa sobre terreno praticamente ao nível do mar, os dois coincidem no primeiro
frame (a saída mostra `alt=3000.0ft`), mas **são grandezas diferentes**: voando para uma serra de
2000 ft, a MSL não muda e a AGL cai para 1000 ft.

### 5. Condições iniciais e `RunIC`

Uma simulação de voo não começa "voando" por acaso. Sem condições iniciais, a aeronave nasce
**parada no chão** e não há nada para o autopiloto controlar. O `FlightModel` monta o estado
inicial assim (`fdm.cpp`):

```cpp
auto* ic = fdm_.GetIC();
ic->SetAltitudeAGLFtIC(3000.0);   // 3000 ft acima do solo
ic->SetVcalibratedKtsIC(90.0);    // 90 nós calibrados — velocidade de cruzeiro da c172p
ic->SetPsiDegIC(90.0);            // proa 90° = leste

fdm_.RunIC();                     // aplica tudo e recalcula o estado consistente

fdm_.GetPropulsion()->InitRunning(-1);              // liga TODOS os motores
fdm_.SetPropertyValue("fcs/mixture-cmd-norm[0]", 1.0);
fdm_.SetPropertyValue("fcs/throttle-cmd-norm[0]", 0.8);
```

Duas armadilhas moram nessas seis linhas, e as duas já morderam este subprojeto:

> ⚠ **`propulsion/engine[0]/set-running` não liga o motor de verdade.** Ele só marca
> `Running = true` no `FGEngine`; magnetos, mistura e RPM continuam onde estavam, e o motor morre
> no primeiro `doEngineStartup()`. `InitRunning(-1)` é o caminho completo (magnetos, mistura, RPM
> inicial e estado estacionário).

> ⚠ **E o motor precisa continuar ligado.** Quem manda no motor a cada frame é o FCS, que empurra
> `fcs/mixture-cmd-norm[0]` para dentro dele em **todo** passo — e essa propriedade nasce em
> `0.0`, que é *mistura cortada*, exatamente o gesto com que se **desliga** um motor a pistão. Sem
> a linha da mistura, o RPM cai de 2726 para 0 em cerca de 9 s de simulação e a Cessna vira
> planador. Era esse o comportamento deste subprojeto antes da correção.

**Trim** é o termo aeronáutico para a condição em que a aeronave voa reta e nivelada **sem
esforço no comando** — momentos aerodinâmicos em equilíbrio. Quanto mais perto do trim você
inicia, menos o autopiloto precisa brigar contra a própria aeronave no primeiro segundo. O
JSBSim sabe calcular trim (`FGTrim`); aqui a condição inicial é boa o bastante e a malha de pitch
absorve o resto — daí a pequena perda de altitude nos primeiros 20 s da saída real.

### 6. Tempo discreto: `dt`, 120 Hz e o gatilho de disparo único

A simulação avança em passos fixos de `dt = 1/120 s ≈ 8,33 ms` — o padrão do JSBSim. Nada é
contínuo: cada `fdm.step()` integra um passo e devolve o controle.

Isso muda como se escreve "faça isto aos 30 segundos". Comparar `sim_time == 30.0` nunca funciona
(ponto flutuante acumulado jamais bate exatamente). E `sim_time >= 30.0` sozinho dispararia
**todo frame** depois dos 30 s. A guarda correta é uma **janela de um passo**:

```cpp
if (sim_time >= 30.0 && sim_time < 30.0 + DT)   // verdadeiro em exatamente UM frame
    ap.set_target_altitude_ft(5000.0);
```

---

## O que é um PID, do zero

O problema: você está a **3000 ft** e quer estar a **5000 ft**. Quanto puxar o manche?

Chame de **erro** a distância até o alvo: `erro = alvo − atual`. Um controlador é uma função que
transforma erro em comando.

### P — proporcional

```cpp
saida = kp * erro;
```

Longe do alvo, comando forte; perto, comando fraco. Simples e quase suficiente — mas o P sozinho
estabiliza **abaixo** do alvo: para manter a subida contra o peso é preciso um comando permanente,
e comando permanente exige erro permanente. Esse resíduo é o **erro estacionário**:

```
alvo ─────────────────────────────────
                     ______________     ← estabiliza aqui e não sobe mais
                  __/                      (erro estacionário)
              ___/
inicial ─────/
```

### P+I — a integral mata o resíduo

```cpp
integral += erro * dt;
saida = kp*erro + ki*integral;
```

A integral é a **soma acumulada** do erro. Enquanto sobrar erro, ela cresce, e o comando cresce
junto — até o erro zerar. O preço é o **windup**:

> ⚠ **Windup** é o que acontece quando a saída satura. O comando de profundor deste autopiloto é
> limitado a ±0,5; se a integral continua acumulando enquanto a saída está travada no limite, ela
> vira um número enorme. Ao chegar no alvo, o controlador precisa "descarregar" essa integral
> inteira antes de reduzir o comando — e a aeronave passa longe do alvo antes de voltar.

A defesa implementada em `autopilot.cpp` é **anti-windup por clamp da integral**:

```cpp
integral_ += error * dt;

// a integral nunca pode, sozinha, exigir mais do que a faixa útil de saída
double max_integral = (out_max_ - out_min_) / (ki_ > 0.0 ? ki_ : 1.0);
integral_ = std::clamp(integral_, -max_integral, max_integral);
```

A conta é direta: se `ki * integral` já cobre toda a faixa `out_max − out_min`, acumular mais não
tem efeito nenhum na saída — só atraso. A guarda `ki_ > 0.0` evita divisão por zero quando o
termo integral está desligado (é o caso da malha de proa, que usa `ki = 0`).

### P+I+D — a derivada freia antes de chegar

```cpp
derivada = (erro - erro_anterior) / dt;
saida = kp*erro + ki*integral + kd*derivada;
```

A derivada do erro é a **velocidade de aproximação** (com sinal trocado). Chegando rápido, ela é
grande e negativa, e reduz o comando **antes** de o alvo chegar — é o freio que corta o
sobressinal (*overshoot*):

```
                 ╭──╮                     ╭─────────────  P+I  (oscila e passa do alvo)
alvo ────────────┼──┼────────────  ───────┼───────────
                ╱   ╰──╯                 ╱                P+I+D (chega e fica)
              ╱                        ╱
```

E, por fim, a **saturação**: toda saída deste projeto passa por
`std::clamp(output, out_min_, out_max_)`. Um profundor não deflete além do batente, e o manete
não passa de 100%. Limitar a saída é o que torna o controlador fisicamente honesto.

---

## Por que estudar isto?

| Sem este subprojeto | Com ele |
|---|---|
| "PID" é uma sigla de tutorial de Arduino | você viu o windup acontecer e sabe por que o clamp resolve |
| controle e física são caixas-pretas separadas | os dois lados são reais: JSBSim é o FDM do FlightGear |
| "o sinal está invertido" é um mistério | você sabe que inverter um sinal é realimentação positiva e a coisa diverge |
| ganhos vêm de um artigo | você ajusta de dentro para fora e vê o efeito de cada um |

Todo drone, piloto automático, estabilizador de câmera, controlador de temperatura ou cruise
control é a mesma estrutura: **um laço de controle rodando sobre um modelo (ou uma planta) que
responde com atraso**. O que muda é a planta.

E há um ganho de C++ puro: `fdm.hpp` é um caso didático de **wrapper RAII sobre uma API C++ velha
e enorme** — o construtor carrega e valida, o destrutor limpa, e o resto do programa fala em
`FlightState` / `ControlInputs`, não em strings de propriedades.

---

## Estrutura de arquivos

```
flight-simulator/
├── README.md          ← este arquivo
├── main.cpp           ← laço de simulação, missão de 240 s, impressão e resumo
├── fdm.hpp / fdm.cpp  ← FlightModel: wrapper RAII sobre JSBSim::FGFDMExec
├── autopilot.hpp/.cpp ← PidController + Autopilot (as três malhas)
├── logger.hpp/.cpp    ← FlightLogger: CSV a cada N frames, fechado no destrutor
├── meson.build        ← executable('flight', …) com jsbsim, fmt e spdlog
└── deps/
    ├── install.sh     ← conan create ./jsbsim  +  git clone dos dados
    ├── jsbsim/
    │   ├── conanfile.py   ← receita Conan que compila a biblioteca
    │   └── data/          ← clone do repositório JSBSim: aircraft/, engine/, systems/
    └── openrti/       ← receita Conan do OpenRTI (não usado por este binário)
```

---

## O código, arquivo por arquivo

### `fdm.hpp` — as duas structs que isolam o resto do programa

```cpp
struct FlightState {                 // o que a aeronave está fazendo
    double altitude_ft   = 0.0;      // MSL
    double latitude_deg  = 0.0;
    double longitude_deg = 0.0;
    double airspeed_kts  = 0.0;      // calibrada
    double vspeed_fpm    = 0.0;      // + = subindo
    double pitch_deg     = 0.0;      // theta
    double roll_deg      = 0.0;      // phi
    double heading_deg   = 0.0;      // psi
    double throttle      = 0.0;      // [0, 1]
    double rpm           = 0.0;
};

struct ControlInputs {               // o que mandamos a aeronave fazer
    double elevator = 0.0;           // [-1, 1]
    double aileron  = 0.0;           // [-1, 1]
    double rudder   = 0.0;           // [-1, 1]
    double throttle = 0.8;           // [ 0, 1]
};
```

O autopiloto **não** conhece o JSBSim: ele recebe um `FlightState` e devolve um `ControlInputs`.
Trocar o FDM por outro (ou por um simulador de brinquedo, num teste) não toca em `autopilot.cpp`.

### `fdm.cpp` — leitura, escrita e duas lições de C++

A leitura converte unidades no caminho:

```cpp
s.airspeed_kts = fdm_.GetPropertyValue("velocities/vc-kts");

// v-down-fps é positivo para BAIXO (o eixo Z local aponta ao solo).
// Invertemos o sinal para a convenção de cockpit (subir = +) e
// multiplicamos por 60 para converter ft/s → ft/min.
s.vspeed_fpm   = -fdm_.GetPropertyValue("velocities/v-down-fps") * 60.0;
```

E o construtor traz um erro clássico, documentado no próprio arquivo: `GetPropulsion()` devolve um
`FGPropulsion*`, mas o cabeçalho do FDM só traz a **declaração adiantada** dessa classe. Guardar e
passar adiante um ponteiro para tipo incompleto é legal; **desreferenciar** não é:

```
error: invalid use of incomplete type 'class JSBSim::FGPropulsion'
note: forward declaration of 'class JSBSim::FGPropulsion'
```

A correção é incluir `JSBSim/models/FGPropulsion.h` só onde o tipo completo é necessário — é o
mesmo mecanismo do *pimpl*, detalhado em [`../linkage/`](../linkage/).

Se `root_dir` vier vazio, `resolve_root` procura `/usr/share/JSBSim`, `/usr/local/share/JSBSim` e
`/opt/JSBSim` (o layout do pacote `.deb`) e lança `std::runtime_error` se não achar nenhum.

### `autopilot.cpp` — três malhas, duas delas em cascata

```
   alvo 5000 ft ─►(erro)─► PID altitude ─► pitch alvo [-7°,+8°] ─►(erro)─► PID pitch ─► elevator
                              ▲ malha EXTERNA (lenta)                ▲ malha INTERNA (rápida)
                              └──────────── theta medido ────────────┴── altitude medida

   alvo 180° ────►(erro+wrap)► PID heading ─► bank alvo [-25°,+25°] ─►(erro)─► PID bank ─► aileron
                                                                                    └─► rudder = aileron·0,1

   alvo 80 kts ──►(erro)─► PID airspeed ─► throttle [0, 1]        (malha única: sem cascata)
```

**Por que cascata.** O profundor não comanda altitude: ele comanda **momento de arfagem**. Entre
"defletir o profundor" e "ganhar altitude" existem duas integrações (momento → atitude →
trajetória) e uma troca de energia com a velocidade. Ligar altitude direto no profundor ignora
essa distância, e o resultado tem nome: **fugóide** — a aeronave sobe, perde velocidade, estola,
mergulha, ganha velocidade, sobe de novo.

Isso não é teoria: era o que este subprojeto fazia. Com a malha única `altitude → elevator`, e o
motor já corrigido, a saída medida ao comandar a subida para 5000 ft era

```
[  30.0s] alt= 3021.0ft  spd= 89.4kts  pitch= +0.8°  ← comando de subida entra aqui
[  40.0s] alt= 3303.8ft  spd= 45.3kts  pitch=-38.4°  roll=+33.8°   ← estolou
[  50.0s] alt= 3038.8ft  spd= 59.5kts  pitch=+51.1°
[ 240.0s] alt= 2870.2ft  spd= 59.5kts  pitch=+15.2°  ← 210 s depois, ainda a 2870 ft
```

Com a cascata, a malha externa decide **que atitude eu quero** (no máximo +8° de nariz em cima,
que a c172p sustenta com folga) e a malha interna cuida de manter essa atitude — que é justamente
o que a superfície de controle sabe fazer.

Os ganhos reais, com os limites de saída (que são parte do projeto, não detalhe):

```cpp
Autopilot::Autopilot()
    : alt_pid_  (0.02,  0.0005, 0.1,   -7.0,  8.0)   // altitude → pitch alvo (graus)
    , pitch_pid_(0.05,  0.01,   0.005, -0.5,  0.5)   // pitch    → elevator
    , hdg_pid_  (1.0,   0.0,    0.0,  -25.0, 25.0)   // heading  → bank alvo (graus)
    , roll_pid_ (0.02,  0.001,  0.005, -0.5,  0.5)   // bank     → aileron
    , spd_pid_  (0.05,  0.005,  0.01,   0.0,  1.0)   // airspeed → throttle
{}
```

Lendo os números como **frases**:

| Malha | Leitura |
|---|---|
| altitude | 100 ft abaixo do alvo pede +2° de nariz em cima; o `kd = 0.1` usa a velocidade vertical para reduzir o pedido antes de chegar |
| pitch | 5° de erro de atitude dão 0,25 de deflexão; o `ki` segura a atitude contra o momento residual |
| heading | 25° de erro de proa pedem 25° de inclinação — já o limite; `ki = 0` porque proa não tem erro estacionário que justifique integral |
| bank | mesma ideia, uma ordem de grandeza menor: aileron é rápido |
| airspeed | 10 kts de erro movem meio manete; a saída já **é** o throttle, limitada a [0, 1] |

> ⚠ **O sinal do profundor é o erro nº 1 de todo sistema de controle.** No JSBSim,
> `fcs/elevator-cmd-norm` **positivo abaixa o nariz**. O erro de pitch é `alvo − atual`, então
> querer subir dá erro **positivo** — e mandar isso direto ao profundor **abaixaria** o nariz.
> Daí o menos:
> ```cpp
> ctrl.elevator = -pitch_pid_.update(pitch_error, dt);
> ```
> Sem ele, a realimentação vira **positiva**: o erro cresce, o comando piora o erro, e a aeronave
> diverge em segundos.

**O wrap de proa.** Ângulos dão a volta, subtração não sabe disso:

```cpp
double hdg_error = target_heading_ - state.heading_deg;
while (hdg_error >  180.0) hdg_error -= 360.0;
while (hdg_error < -180.0) hdg_error += 360.0;
```

Voando a **350°** com alvo **10°**: o erro cru é `10 − 350 = −340°`, e o autopiloto viraria quase
uma volta inteira **à esquerda**. Depois do wrap, o erro é `+20°` — vinte graus à direita, que é o
caminho certo.

**Coordenação de curva.** `ctrl.rudder = ctrl.aileron * 0.1;` — uma curva feita só com aileron
**derrapa**: o nariz aponta para fora da trajetória, o passageiro sente a bola do inclinômetro sair
do centro e o arrasto sobe. O leme, na proporção certa, alinha o nariz. Um autopiloto de verdade
calcula isso a partir do escorregamento (`aero/beta-deg`); esta é a versão simplificada, e o
valor 0,1 é empírico.

### `main.cpp` — o laço, e uma sutileza de um frame

```cpp
constexpr double DT              = 1.0 / 120.0;   // JSBSim roda a 120 Hz
constexpr double SIM_END         = 240.0;
constexpr double PRINT_INTERVAL_S = 10.0;         // em SEGUNDOS
int print_frame = static_cast<int>(PRINT_INTERVAL_S / DT);   // = 1200 frames

while (sim_time <= SIM_END) {
    if (sim_time >= 30.0  && sim_time < 30.0 + DT) ap.set_target_altitude_ft(5000.0);
    if (sim_time >= 90.0  && sim_time < 90.0 + DT) ap.set_target_heading_deg(180.0);
    if (sim_time >= 180.0 && sim_time < 180.0 + DT) ap.set_target_airspeed_kts(80.0);

    auto state = fdm.state();          // 1) lê os sensores
    auto ctrl  = ap.update(state, DT); // 2) decide
    fdm.set_controls(ctrl);            // 3) aplica

    if (!fdm.step()) break;            // 4) integra 1/120 s

    logger.record(sim_time, state);    // 5) grava
    if (frame % print_frame == 0) print_state(sim_time, state);

    sim_time += DT;
    ++frame;
}
```

> ⚠ **Defasagem de um frame.** `state` é lido **antes** do `step()` e registrado **depois**. A
> linha do CSV rotulada `t = 30.00` contém, portanto, o estado de `t = 30.00` *no instante em que
> a decisão foi tomada* — o estado resultante daquele passo só aparece na linha seguinte. Para
> um passo de 8,33 ms isso é irrelevante na prática, mas é exatamente o tipo de detalhe que faz
> um gráfico "não bater" com a expectativa.

### `logger.cpp` — RAII e taxa de gravação

`FlightLogger` abre o arquivo no construtor (lançando se falhar), escreve o cabeçalho e **fecha no
destrutor**. `every_n_frames = 12` significa uma linha a cada 12 frames = **100 ms**; em 240 s de
missão saem **2400 registros** (é o número que o programa imprime no fim).

---

## Saída esperada

Executando `./dist/bin/flight` a partir da raiz (a saída é determinística — estes números se
repetem a cada execução):

```
 time(s)    alt(ft)   spd(kts)    vs(fpm)  pitch(°)   roll(°)    hdg(°)    thr     rpm
------------------------------------------------------------------------------------------
[   0.0s] alt= 3000.0ft  spd= 90.0kts  vs=    -0fpm  pitch= +0.0°  roll= +0.0°  hdg= 90.0°  thr=0.80  rpm=2726
[  10.0s] alt= 2983.9ft  spd= 81.8kts  vs=  -290fpm  pitch= +0.4°  roll= -0.5°  hdg= 88.8°  thr=0.60  rpm=1758
[  20.0s] alt= 2957.4ft  spd= 86.3kts  vs=   -53fpm  pitch= +1.4°  roll= +0.5°  hdg= 88.4°  thr=0.68  rpm=2098
[  30.0s] alt= 2954.8ft  spd= 88.7kts  vs=    +6fpm  pitch= +1.5°  roll= +0.4°  hdg= 89.1°  thr=0.68  rpm=2092   ← alvo vira 5000 ft
[  40.0s] alt= 3112.3ft  spd= 87.8kts  vs= +1090fpm  pitch= +8.2°  roll= +0.6°  hdg= 88.1°  thr=0.90  rpm=2768
[  50.0s] alt= 3297.4ft  spd= 89.3kts  vs= +1120fpm  pitch= +8.1°  roll= +0.7°  hdg= 88.8°  thr=0.88  rpm=2730
[  60.0s] alt= 3484.6ft  spd= 89.8kts  vs= +1126fpm  pitch= +8.0°  roll= +0.5°  hdg= 89.2°  thr=0.88  rpm=2723
[  70.0s] alt= 3672.4ft  spd= 89.9kts  vs= +1129fpm  pitch= +8.0°  roll= +0.4°  hdg= 89.4°  thr=0.88  rpm=2727
[  80.0s] alt= 3860.7ft  spd= 89.9kts  vs= +1130fpm  pitch= +8.0°  roll= +0.4°  hdg= 89.5°  thr=0.89  rpm=2731
[  90.0s] alt= 4049.3ft  spd= 89.9kts  vs= +1133fpm  pitch= +8.0°  roll= +0.4°  hdg= 89.6°  thr=0.89  rpm=2739   ← alvo vira proa 180°
[ 100.0s] alt= 4211.0ft  spd= 90.0kts  vs=  +967fpm  pitch= +7.8°  roll=+26.1°  hdg=134.2°  thr=0.88  rpm=2701
[ 110.0s] alt= 4386.4ft  spd= 89.6kts  vs= +1174fpm  pitch= +8.3°  roll= +6.7°  hdg=175.1°  thr=0.91  rpm=2764
[ 120.0s] alt= 4579.8ft  spd= 89.9kts  vs= +1151fpm  pitch= +8.1°  roll= +0.8°  hdg=179.0°  thr=0.91  rpm=2768
[ 130.0s] alt= 4771.1ft  spd= 89.9kts  vs= +1146fpm  pitch= +8.0°  roll= +0.5°  hdg=179.3°  thr=0.91  rpm=2769
[ 140.0s] alt= 4962.2ft  spd= 89.9kts  vs= +1147fpm  pitch= +8.0°  roll= +0.4°  hdg=179.5°  thr=0.92  rpm=2775
[ 150.0s] alt= 5153.7ft  spd= 89.8kts  vs= +1150fpm  pitch= +8.0°  roll= +0.4°  hdg=179.5°  thr=0.93  rpm=2783
[ 160.0s] alt= 5328.3ft  spd= 91.1kts  vs=  +811fpm  pitch= +5.7°  roll= +0.6°  hdg=179.7°  thr=0.86  rpm=2663
[ 170.0s] alt= 5419.3ft  spd= 91.6kts  vs=  +301fpm  pitch= +2.7°  roll= +0.3°  hdg=180.4°  thr=0.76  rpm=2333
[ 180.0s] alt= 5437.7ft  spd= 91.5kts  vs=   -56fpm  pitch= +0.7°  roll= +0.1°  hdg=180.5°  thr=0.69  rpm=2120   ← alvo vira 80 kts
[ 190.0s] alt= 5393.0ft  spd= 81.8kts  vs=  -449fpm  pitch= -0.6°  roll= -0.4°  hdg=180.9°  thr=0.36  rpm=1416
[ 200.0s] alt= 5316.2ft  spd= 79.3kts  vs=  -455fpm  pitch= -0.3°  roll= -0.2°  hdg=180.1°  thr=0.48  rpm=1572
[ 210.0s] alt= 5243.5ft  spd= 79.3kts  vs=  -416fpm  pitch= -0.1°  roll= +0.1°  hdg=179.8°  thr=0.52  rpm=1658
[ 220.0s] alt= 5177.6ft  spd= 79.5kts  vs=  -374fpm  pitch= +0.2°  roll= +0.1°  hdg=179.8°  thr=0.53  rpm=1690
[ 230.0s] alt= 5119.3ft  spd= 79.6kts  vs=  -324fpm  pitch= +0.5°  roll= +0.1°  hdg=179.8°  thr=0.55  rpm=1725
[ 240.0s] alt= 5070.3ft  spd= 79.5kts  vs=  -263fpm  pitch= +0.9°  roll= +0.1°  hdg=179.8°  thr=0.57  rpm=1774
------------------------------------------------------------------------------------------

Simulação concluída.
  Tempo simulado  : 240.0 s
  Tempo real      : 1.79 s  (134x realtime)
  Registros CSV   : 2400
  Arquivo gerado  : flight_data.csv
```

Como ler essa tabela:

- **0–30 s**: mantendo 3000 ft. Perde ~45 ft no começo (a condição inicial não está em trim
  perfeito) e a malha de pitch recupera; o manete cai de 0,80 para ~0,68, que é o necessário para
  segurar 90 kts nivelado.
- **30–150 s**: subida. A malha externa satura no limite de **+8° de pitch** e a aeronave sobe a
  ~1130 ft/min constantes, com a velocidade **presa em 90 kts** pela malha de manete — é assim
  que se sobe de verdade: atitude constante, velocidade constante, manete faz o resto.
- **90–120 s**: a curva. `roll` vai a **+26°** (o comando é limitado a 25°; o excedente é o
  sobressinal da malha interna), a proa vai de 90° a 180° em ~30 s e o `roll` volta a zero. A
  subida não é interrompida.
- **150–180 s**: chegada. A malha externa reduz o pitch alvo de 8° para ~0,7° e a subida termina
  com **sobressinal de ~438 ft** (pico de 5438,6 ft aos 177,9 s).
- **180–240 s**: reduzir a velocidade para 80 kts custa altitude — o manete cai a 0,36 e a
  aeronave desce dos 5438 ft de volta para 5070 ft, ainda convergindo quando a missão acaba.
  **Este é o comportamento correto de um autopiloto simples**: as malhas de energia (velocidade) e
  de altitude não estão acopladas, então mexer numa perturba a outra.

---

## Como validar

Rode a partir da raiz e confira o CSV — 2400 linhas, colunas
`time_s,altitude_ft,airspeed_kts,vspeed_fpm,pitch_deg,roll_deg,heading_deg,throttle,rpm`.

**1. A subida chega e não passa muito.**

```bash
gnuplot -e "set datafile separator ','; set term dumb; \
            plot 'flight_data.csv' using 1:2 with lines title 'alt (ft)'"
```

Esperado: sobe de 3000 a 5000 ft, cruza os 5000 ft em **t ≈ 142 s**, atinge o pico de
**5438,6 ft em t ≈ 177,9 s** e volta a cair na direção do alvo. Sem gnuplot:

```bash
awk -F, 'NR>1 && $2>m {m=$2; t=$1} END {print "pico:", m, "ft em t =", t, "s"}' flight_data.csv
```

**2. A curva é limpa.** A proa vai de 90° a ~180° sem oscilar e a inclinação respeita o limite:

```bash
awk -F, 'NR>1 {if($7<hmin||NR==2)hmin=$7; if($7>hmax)hmax=$7;
               r=($6<0?-$6:$6); if(r>rmax)rmax=r}
         END {print "hdg:", hmin, "→", hmax, " |roll| máx:", rmax}' flight_data.csv
```

Esperado: `hdg: 88.0 → 181.2` e `|roll| máx: 26.26` — coerente com o limite de 25° da malha externa
mais um pequeno sobressinal da interna.

**3. A saturação funciona.** O manete jamais sai de [0, 1] — é a prova do `std::clamp` no PID:

```bash
awk -F, 'NR>1 {if($8<lo||NR==2)lo=$8; if($8>hi)hi=$8} END {print "throttle:", lo, "…", hi}' \
    flight_data.csv
```

Esperado: `throttle: 0.000 … 0.932`.

**4. O motor fica vivo.** `rpm` (coluna 9) nunca chega a zero depois do primeiro frame. Se chegar,
alguém removeu a linha da mistura em `fdm.cpp`.

**Exercícios que quebram o autopiloto de propósito** (desfaça depois):

1. **Sinal invertido** — troque `ctrl.elevator = -pitch_pid_.update(...)` por `+`. A aeronave
   diverge em segundos: é realimentação positiva.
2. **Sem cascata** — ligue a altitude direto no profundor
   (`ctrl.elevator = -alt_pid_.update(alt_error, dt);` com limites ±0,5). Você reproduz a fugóide
   documentada acima: pitch oscilando entre −38° e +51°, e a altitude parada.
3. **Sem anti-windup** — comente as duas linhas do `std::clamp` da integral e observe o
   sobressinal da subida crescer.
4. **Ganho 10× maior** — `alt_pid_(0.2, …)`. A malha externa passa a pedir o limite de pitch a
   qualquer erro e a aeronave vira um pêndulo.
5. **Ajuste seu** — o roteiro prático de sintonia é: zere `ki` e `kd`, suba `kp` até aparecer
   oscilação sustentada, recue para ~metade, acrescente `kd` até o sobressinal sumir e só então
   acrescente `ki` para matar o erro residual. É a versão de bancada do método de
   Ziegler–Nichols. E **sempre de dentro para fora**: primeiro a malha de atitude, depois a de
   altitude.

---

## Referências

Neste repositório:

- [`../linkage/`](../linkage/) — declaração adiantada, tipo incompleto e pimpl: o erro de
  `FGPropulsion` explicado por inteiro
- [`../calculus/`](../calculus/) — integração numérica, que é o que o `step()` faz 120 vezes por
  segundo
- [`../errors/`](../errors/) — exceções em construtores, o padrão usado por `FlightModel` e
  `FlightLogger`
- [`../file_io/`](../file_io/) — `std::ofstream` e escrita de arquivos, base do `FlightLogger`
- [`../chrono/`](../chrono/) — `steady_clock`, usado no resumo de tempo real do `main.cpp`

Fontes canônicas:

- *JSBSim Reference Manual* (PDF no repositório JSBSim-Team/jsbsim) — o documento definitivo
- A lista completa de propriedades da árvore está no manual e em `data/aircraft/c172p/c172p.xml`
- `github.com/JSBSim-Team/jsbsim` — código, dados de aeronaves e exemplos
- Åström & Murray, *Feedback Systems* — capítulos sobre PID, saturação e anti-windup
- *Airplane Flying Handbook* (FAA) — curva coordenada, trim e velocidade calibrada, do lado do
  piloto
