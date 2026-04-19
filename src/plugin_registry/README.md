# plugin_registry — Sistema de Plugins com Carregamento Dinâmico

## O que é este projeto?

Implementa um **sistema de plugins** completo que carrega bibliotecas compartilhadas (`.so`) em runtime usando `dlopen`/`dlsym`. Plugins (controladores PID e LQR) são descobertos automaticamente e podem ser trocados sem recompilar a aplicação principal.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| `dlopen`/`dlsym`/`dlclose` | API POSIX para carregamento dinâmico de bibliotecas |
| Interface virtual pura | Contrato entre host e plugins |
| Singleton registry | Gerencia todos os plugins carregados |
| `unique_ptr` com deleter customizado | Plugin criado e destruído pelo próprio `.so` |
| Alocador correctness | Objeto alocado no `.so` deve ser destruído pelo `.so` |

---

## Estrutura de arquivos

```
plugin_registry/
├── main.cpp                      ← descobre plugins, roda episódios de controle
├── icontroller.hpp               ← interface IController (contrato dos plugins)
├── plugin_registry.hpp           ← ControllerRegistry: singleton + dlopen/dlsym
└── plugins/
    ├── pid_controller.cpp        ← plugin PID (compilado como .so)
    └── lqr_controller.cpp        ← plugin LQR (compilado como .so)
```

---

## A Interface — `icontroller.hpp`

```cpp
namespace plugin {

// Estado do sistema (lido dos sensores)
struct State {
    double altitude_ft = 3000.0;
    double speed_kts   = 90.0;
    double pitch_deg   = 0.0;
};

// Ação de controle (aplicada aos atuadores)
struct Action {
    double elevator = 0.0;   // profundor: −1 a +1
    double throttle = 0.75;  // acelerador: 0 a 1
};

// Interface que todos os plugins DEVEM implementar
class IController {
public:
    virtual ~IController() = default;

    virtual const char* name() const = 0;
    virtual void reset() = 0;
    virtual Action compute(const State& state) = 0;
};

} // namespace plugin
```

---

## Os plugins — `pid_controller.cpp`

```cpp
#include "../icontroller.hpp"

class PIDController : public plugin::IController {
    // Parâmetros do PID
    double Kp_ = 0.01, Ki_ = 0.001, Kd_ = 0.005;
    double integral_ = 0.0, prev_error_ = 0.0;

public:
    const char* name() const override { return "pid"; }

    void reset() override {
        integral_ = prev_error_ = 0.0;
    }

    plugin::Action compute(const plugin::State& s) override {
        double error = 3000.0 - s.altitude_ft; // target altitude
        integral_ += error * 0.1;
        double deriv = error - prev_error_;
        prev_error_ = error;

        plugin::Action a;
        a.elevator = Kp_ * error + Ki_ * integral_ + Kd_ * deriv;
        a.throttle = 0.75 + 0.01 * (90.0 - s.speed_kts);
        return a;
    }
};

// ── Símbolos exportados — obrigatórios para o registry ──────────────────────
// extern "C" para evitar name mangling (dlsym usa nomes C simples)

extern "C" const char* plugin_name() { return "pid"; }

extern "C" plugin::IController* create_controller() {
    return new PIDController();
}

extern "C" void destroy_controller(plugin::IController* p) {
    delete p;
}
```

---

## O Registry — `plugin_registry.hpp`

### Carregamento com `dlopen`

```cpp
void load_plugin(const std::string& path) {
    // RTLD_NOW: resolve todos os símbolos ao carregar (falha cedo se algo falta)
    // RTLD_LOCAL: símbolos não vazam para outros .so
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        throw std::runtime_error("dlopen falhou: " + std::string(dlerror()));
    }

    // dlsym: busca símbolo por nome → retorna void*
    // dlerror() DEVE ser chamado após cada dlsym para detectar erro
    // (dlsym pode retornar nullptr legitimamente se o símbolo vale nullptr)
    dlerror(); // limpa erro anterior
    auto fn_name = reinterpret_cast<FnPluginName>(dlsym(handle, "plugin_name"));
    if (const char* err = dlerror()) {
        dlclose(handle);
        throw std::runtime_error("plugin_name não encontrado: " + std::string(err));
    }

    // Armazena handle + funções de criação/destruição
    plugins_[fn_name()] = PluginHandle{handle, fn_name(), fn_create, fn_destroy};
}
```

### Instanciação com deleter customizado

```cpp
// unique_ptr<IController, FnDestroyController>
// O segundo template arg é o tipo do deleter!
std::unique_ptr<IController, FnDestroyController>
create(const std::string& name) {
    const PluginHandle& ph = plugins_.at(name);
    IController* raw = ph.create();

    // Em vez de "delete ptr", o unique_ptr chamará "ph.destroy(ptr)"
    // CRÍTICO: objeto alocado no .so → DEVE ser destruído pelo .so
    // Misturar allocators (host aloca, plugin destrói) = UNDEFINED BEHAVIOR
    return { raw, ph.destroy };
}
```

### Por que o deleter customizado é crítico?

```
Cenário problemático:
  Host (main):      usa allocator do sistema (libstdc++.so)
  Plugin (pid.so):  pode ter seu próprio allocator se staticamente linkado

Se o HOST fizer "delete ptr" para objeto alocado no PLUGIN:
→ Chama o destrutor no contexto do HOST
→ Libera memória com o allocator do HOST
→ O PLUGIN alocou com SEU allocator → CORRUPÇÃO DE HEAP / CRASH
```

A solução: o plugin exporta `destroy_controller` que é chamada pelo host — o `.so` destrói o que ele mesmo criou.

---

## Descoberta automática de plugins

```cpp
int load_directory(const std::string& dir_path) {
    DIR* dir = opendir(dir_path.c_str());
    int loaded = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string fname = entry->d_name;
        if (!ends_with(fname, ".so")) continue; // filtra só .so

        try {
            load_plugin(dir_path + "/" + fname);
            ++loaded;
        } catch (const std::exception& e) {
            // Plugin ruim não derruba os outros
            std::cerr << "[aviso] " << e.what() << "\n";
        }
    }

    closedir(dir);
    return loaded;
}
```

---

## Fluxo completo de uso

```cpp
// 1. Descobre e carrega todos os .so no diretório
auto& reg = plugin::ControllerRegistry::instance();
reg.load_directory("plugins");
// Saída: [registry] plugin 'pid' carregado de plugins/libpid.so
//        [registry] plugin 'lqr' carregado de plugins/liblqr.so

// 2. Lista plugins disponíveis
for (const auto& name : reg.available()) {
    fmt::print("  - {}\n", name);
}

// 3. Cria instâncias e usa
auto ctrl = reg.create("pid"); // unique_ptr com deleter customizado
ctrl->reset();
plugin::Action action = ctrl->compute(state);

// 4. Troca de controlador em runtime — ZERO recompilação
ctrl = reg.create("lqr");  // muda o algoritmo instantaneamente
ctrl->reset();
action = ctrl->compute(state);
```

---

## Saída esperada

```
=== Plugin Registry Demo ===

Varrendo 'plugins'...
[registry] plugin 'pid' carregado de plugins/libpid.so
[registry] plugin 'lqr' carregado de plugins/liblqr.so
2 plugin(s) carregado(s).

Plugins disponíveis:
  - pid
  - lqr

=== Controlador: pid ===
  step 01   alt= 2800.0 ft  spd=85.0 kts  pitch=-2.0° | elev=+0.020  thr=0.800
  step 02   alt= 2801.4 ft  spd=85.2 kts  pitch=-1.9° | elev=+0.019  thr=0.798
  ...

=== Troca de controlador em runtime ===
  [pid] alt= 2900.0 ft  ...
  [lqr] alt= 2901.2 ft  ...  ← mesmo estado, algoritmo diferente
  [pid] alt= 2902.1 ft  ...
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja plugin_registry

# Os plugins são compilados como .so separados
# Executar apontando para o diretório de plugins:
./bin/plugin_registry dist/plugins
```

---

## Dependências

- `dl` (libdl) — `dlopen`/`dlsym`/`dlclose`
- `fmt` — formatação de saída
