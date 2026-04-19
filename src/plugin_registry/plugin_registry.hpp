#pragma once

#include "icontroller.hpp"

#include <dlfcn.h>      // dlopen, dlsym, dlclose, RTLD_NOW
#include <dirent.h>     // opendir, readdir, closedir, dirent
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <iostream>

namespace plugin {

// ─────────────────────────────────────────────────────────────────────────────
// Tipos dos símbolos que o registry busca em cada .so via dlsym
//
// dlsym retorna void* — sem informação de tipo. Por isso declaramos os
// tipos de ponteiro de função explicitamente e fazemos reinterpret_cast.
// É unsafe por natureza: se o .so exportar a função com assinatura errada,
// o comportamento é indefinido. O contrato está na macro REGISTER_CONTROLLER.
// ─────────────────────────────────────────────────────────────────────────────

using FnPluginName      = const char*    (*)();
using FnCreateController = IController* (*)();
using FnDestroyController = void        (*)(IController*);

// ─────────────────────────────────────────────────────────────────────────────
// PluginHandle
//
// Agrupa tudo que o registry precisa saber sobre um .so carregado:
// o handle do dlopen, as funções de criação/destruição, e o nome canônico.
//
// O handle é void* — dlopen retorna isso. Não é um ponteiro para objeto,
// é um token opaco que identifica a biblioteca no sistema operacional.
// ─────────────────────────────────────────────────────────────────────────────

struct PluginHandle {
    void*               dl_handle = nullptr;
    std::string         name;
    FnCreateController  create    = nullptr;
    FnDestroyController destroy   = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// ControllerRegistry
//
// Singleton que:
//   1. Varre um diretório procurando arquivos .so
//   2. Carrega cada .so com dlopen
//   3. Resolve os símbolos plugin_name / create_controller / destroy_controller
//   4. Mapeia nome canônico → PluginHandle
//   5. Cria instâncias sob demanda via create()
//   6. Fecha todos os handles no destrutor
// ─────────────────────────────────────────────────────────────────────────────

class ControllerRegistry {
public:
    // Singleton: uma única instância global.
    static ControllerRegistry& instance() {
        static ControllerRegistry reg;
        return reg;
    }

    // Não copiável nem movível — há handles de SO abertos.
    ControllerRegistry(const ControllerRegistry&) = delete;
    ControllerRegistry& operator=(const ControllerRegistry&) = delete;

    // ── Carregamento ─────────────────────────────────────────────────────────

    // Varre 'dir_path' e carrega todos os .so encontrados.
    // Retorna o número de plugins carregados com sucesso.
    int load_directory(const std::string& dir_path) {
        // opendir abre um stream de diretório — análogo a fopen para arquivos.
        DIR* dir = opendir(dir_path.c_str());
        if (!dir) {
            throw std::runtime_error(
                "Não foi possível abrir o diretório: " + dir_path);
        }

        int loaded = 0;

        // readdir retorna um ponteiro para dirent a cada chamada,
        // null quando não há mais entradas. É uma API C clássica.
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string fname = entry->d_name;

            // Filtra só arquivos .so
            if (!ends_with(fname, ".so")) continue;

            std::string full_path = dir_path + "/" + fname;
            try {
                load_plugin(full_path);
                ++loaded;
            } catch (const std::exception& e) {
                // Um plugin ruim não derruba os outros.
                std::cerr << "[registry] aviso: " << e.what() << "\n";
            }
        }

        closedir(dir);
        return loaded;
    }

    // Carrega um .so específico por caminho completo.
    void load_plugin(const std::string& path) {
        // dlopen carrega o .so no espaço de endereço do processo.
        //
        // RTLD_NOW: resolve todos os símbolos imediatamente ao carregar.
        //   Alternativa: RTLD_LAZY — resolve só quando o símbolo é chamado.
        //   Usamos RTLD_NOW para falhar cedo se houver símbolo faltando.
        //
        // RTLD_LOCAL: os símbolos do .so não ficam visíveis para outros .so.
        //   Alternativa: RTLD_GLOBAL — símbolos ficam globais (necessário
        //   se plugins dependem uns dos outros). Aqui não precisamos disso.
        void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            // dlerror() retorna a mensagem de erro do último dlopen/dlsym.
            throw std::runtime_error(
                "dlopen falhou em '" + path + "': " + dlerror());
        }

        // ── Resolução de símbolos via dlsym ──────────────────────────────
        //
        // dlsym busca um símbolo por nome dentro do handle.
        // Retorna void* — fazemos reinterpret_cast para o tipo correto.
        //
        // dlerror() deve ser chamado APÓS dlsym para verificar erro:
        // dlsym pode retornar nullptr legitimamente (símbolo vale nullptr),
        // então checar só o retorno não é suficiente.

        // Limpa erro anterior antes de cada dlsym.
        dlerror();

        auto fn_name = reinterpret_cast<FnPluginName>(
            dlsym(handle, "plugin_name"));
        if (const char* err = dlerror()) {
            dlclose(handle);
            throw std::runtime_error(
                "plugin_name não encontrado em '" + path + "': " + err);
        }

        dlerror();
        auto fn_create = reinterpret_cast<FnCreateController>(
            dlsym(handle, "create_controller"));
        if (const char* err = dlerror()) {
            dlclose(handle);
            throw std::runtime_error(
                "create_controller não encontrado em '" + path + "': " + err);
        }

        dlerror();
        auto fn_destroy = reinterpret_cast<FnDestroyController>(
            dlsym(handle, "destroy_controller"));
        if (const char* err = dlerror()) {
            dlclose(handle);
            throw std::runtime_error(
                "destroy_controller não encontrado em '" + path + "': " + err);
        }

        // ── Registro ─────────────────────────────────────────────────────
        std::string canon_name = fn_name();

        if (plugins_.count(canon_name)) {
            std::cerr << "[registry] aviso: plugin '" << canon_name
                      << "' já registrado, ignorando " << path << "\n";
            dlclose(handle);
            return;
        }

        plugins_[canon_name] = PluginHandle{
            .dl_handle = handle,
            .name      = canon_name,
            .create    = fn_create,
            .destroy   = fn_destroy,
        };

        std::cout << "[registry] plugin '" << canon_name
                  << "' carregado de " << path << "\n";
    }

    // ── Instanciação ─────────────────────────────────────────────────────────

    // Cria uma instância do controlador pelo nome canônico.
    //
    // Retorna unique_ptr com deleter customizado: em vez de chamar
    // operator delete (que usaria o allocator do host), chamamos
    // destroy_controller do próprio .so — que usa o allocator correto.
    //
    // Isso é crítico: objeto alocado dentro do .so deve ser destruído
    // pelo mesmo .so. Misturar allocators é UB.
    std::unique_ptr<IController, FnDestroyController>
    create(const std::string& name) {
        auto it = plugins_.find(name);
        if (it == plugins_.end()) {
            throw std::runtime_error(
                "Plugin não encontrado: '" + name + "'");
        }

        const PluginHandle& ph = it->second;
        IController* raw = ph.create();
        if (!raw) {
            throw std::runtime_error(
                "create_controller retornou nullptr para '" + name + "'");
        }

        // O segundo argumento do unique_ptr é o deleter:
        // em vez de `delete ptr`, ele chama `ph.destroy(ptr)`.
        return { raw, ph.destroy };
    }

    // ── Consulta ─────────────────────────────────────────────────────────────

    std::vector<std::string> available() const {
        std::vector<std::string> names;
        names.reserve(plugins_.size());
        for (const auto& [name, _] : plugins_)
            names.push_back(name);
        return names;
    }

    bool has(const std::string& name) const {
        return plugins_.count(name) > 0;
    }

private:
    ControllerRegistry() = default;

    // Destrutor fecha todos os handles — o SO descarrega os .so.
    // Importante: os unique_ptrs devem ser destruídos ANTES disso,
    // pois após dlclose os ponteiros de função ficam inválidos.
    ~ControllerRegistry() {
        for (auto& [name, ph] : plugins_) {
            dlclose(ph.dl_handle);
        }
    }

    std::unordered_map<std::string, PluginHandle> plugins_;

    // ── Helper ───────────────────────────────────────────────────────────────
    static bool ends_with(const std::string& s, const std::string& suffix) {
        if (s.size() < suffix.size()) return false;
        return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
};

} // namespace plugin
