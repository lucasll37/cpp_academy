#pragma once

#include <string>

namespace plugin {

// ─────────────────────────────────────────────────────────────────────────────
// Tipos de dados trafegados pela ABI boundary
//
// Usamos structs simples (POD) intencionalmente: não há vtable, não há
// alocação oculta, não há problema de ABI entre unidades de compilação
// diferentes. São seguras para cruzar a fronteira .so ↔ host.
// ─────────────────────────────────────────────────────────────────────────────

struct State {
    double altitude_ft  = 0.0;
    double speed_kts    = 0.0;
    double pitch_deg    = 0.0;
    double roll_deg     = 0.0;
    double heading_deg  = 0.0;
};

struct Action {
    double elevator  = 0.0;   // [-1, 1]
    double throttle  = 0.0;   // [ 0, 1]
    double aileron   = 0.0;   // [-1, 1]
};

// ─────────────────────────────────────────────────────────────────────────────
// Interface abstrata do controlador
//
// Todo plugin deve herdar desta classe. Os métodos virtuais definem o contrato
// que o host (main.cpp) usa — sem saber nada da implementação concreta.
// ─────────────────────────────────────────────────────────────────────────────

class IController {
public:
    virtual ~IController() = default;

    // Retorna o nome canônico deste controlador (ex: "pid", "lqr").
    virtual std::string name() const = 0;

    // Reinicia o estado interno (integradores, histórico, etc.).
    // Chamado antes de cada episódio — análogo ao reset() do Gymnasium.
    virtual void reset() = 0;

    // Dado o estado atual da aeronave, retorna a ação de controle.
    virtual Action compute(const State& state) = 0;
};

} // namespace plugin


// ─────────────────────────────────────────────────────────────────────────────
// Macro REGISTER_CONTROLLER
//
// Uso:
//   REGISTER_CONTROLLER(MinhaClasse, "meu_nome")
//
// Expande para três funções com ligação C (extern "C"), que o registry
// localiza via dlsym(). A ligação C desativa o name mangling do C++,
// garantindo que o símbolo no .so tenha exatamente o nome esperado.
//
// Por que macro e não template ou função normal?
//   - Templates têm name mangling → dlsym não funciona.
//   - Função normal exigiria que o plugin registrasse a si mesmo
//     em algum ponto de inicialização — mais complexo e frágil.
//   - A macro resolve em tempo de pré-processamento: zero custo runtime,
//     zero infraestrutura extra, zero risco de esquecimento.
//
// As \ no final de cada linha são continuações de linha: dizem ao
// pré-processador que a macro ainda não terminou.
// ─────────────────────────────────────────────────────────────────────────────

#define REGISTER_CONTROLLER(ClassName, name_str)                        \
    extern "C" __attribute__((visibility("default")))                   \
    const char* plugin_name() {                                         \
        return name_str;                                                 \
    }                                                                   \
    extern "C" __attribute__((visibility("default")))                   \
    plugin::IController* create_controller() {                          \
        return new ClassName();                                          \
    }                                                                   \
    extern "C" __attribute__((visibility("default")))                   \
    void destroy_controller(plugin::IController* p) {                   \
        delete p;                                                        \
    }


// #pragma once

// #include <string>

// namespace plugin {

// // ─────────────────────────────────────────────────────────────────────────────
// // Tipos de dados trafegados pela ABI boundary
// //
// // Usamos structs simples (POD) intencionalmente: não há vtable, não há
// // alocação oculta, não há problema de ABI entre unidades de compilação
// // diferentes. São seguras para cruzar a fronteira .so ↔ host.
// // ─────────────────────────────────────────────────────────────────────────────

// struct State {
//     double altitude_ft  = 0.0;
//     double speed_kts    = 0.0;
//     double pitch_deg    = 0.0;
//     double roll_deg     = 0.0;
//     double heading_deg  = 0.0;
// };

// struct Action {
//     double elevator  = 0.0;   // [-1, 1]
//     double throttle  = 0.0;   // [ 0, 1]
//     double aileron   = 0.0;   // [-1, 1]
// };

// // ─────────────────────────────────────────────────────────────────────────────
// // Interface abstrata do controlador
// //
// // Todo plugin deve herdar desta classe. Os métodos virtuais definem o contrato
// // que o host (main.cpp) usa — sem saber nada da implementação concreta.
// // ─────────────────────────────────────────────────────────────────────────────

// class IController {
// public:
//     virtual ~IController() = default;

//     // Retorna o nome canônico deste controlador (ex: "pid", "lqr").
//     virtual std::string name() const = 0;

//     // Reinicia o estado interno (integradores, histórico, etc.).
//     // Chamado antes de cada episódio — análogo ao reset() do Gymnasium.
//     virtual void reset() = 0;

//     // Dado o estado atual da aeronave, retorna a ação de controle.
//     virtual Action compute(const State& state) = 0;
// };

// } // namespace plugin


// // ─────────────────────────────────────────────────────────────────────────────
// // Macro REGISTER_CONTROLLER
// //
// // Uso:
// //   REGISTER_CONTROLLER(MinhaClasse, "meu_nome")
// //
// // Expande para três funções com ligação C (extern "C"), que o registry
// // localiza via dlsym(). A ligação C desativa o name mangling do C++,
// // garantindo que o símbolo no .so tenha exatamente o nome esperado.
// //
// // Por que macro e não template ou função normal?
// //   - Templates têm name mangling → dlsym não funciona.
// //   - Função normal exigiria que o plugin registrasse a si mesmo
// //     em algum ponto de inicialização — mais complexo e frágil.
// //   - A macro resolve em tempo de pré-processamento: zero custo runtime,
// //     zero infraestrutura extra, zero risco de esquecimento.
// //
// // As \ no final de cada linha são continuações de linha: dizem ao
// // pré-processador que a macro ainda não terminou.
// // ─────────────────────────────────────────────────────────────────────────────

// #define REGISTER_CONTROLLER(ClassName, name_str)                        \
//                                                                         \
//     /* Retorna o nome canônico. O registry usa isso para mapear        \
//        "pid" → handle do pid.so, sem precisar do nome do arquivo. */   \
//     extern "C" const char* plugin_name() {                             \
//         return name_str;                                                \
//     }                                                                   \
//                                                                         \
//     /* Aloca e retorna uma instância nova. O host não chama new        \
//        diretamente — toda alocação fica dentro do .so, garantindo      \
//        que o mesmo allocator que aloca também libera. */               \
//     extern "C" plugin::IController* create_controller() {             \
//         return new ClassName();                                         \
//     }                                                                   \
//                                                                         \
//     /* Libera a instância. O unique_ptr do host usa este símbolo como  \
//        deleter customizado, em vez do delete padrão. */                \
//     extern "C" void destroy_controller(plugin::IController* p) {      \
//         delete p;                                                       \
//     }
