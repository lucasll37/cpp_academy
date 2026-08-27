#pragma once
// =============================================================================
//  adaptador.hpp — as DUAS formas de adaptar
//
//  O GoF descreve duas variantes, e a diferença é como o adaptador se liga ao
//  adaptado:
//
//    ADAPTER DE OBJETO      — o adaptador CONTÉM o adaptado (composição)
//    ADAPTER DE CLASSE      — o adaptador HERDA do adaptado (herança múltipla)
//
//  Em C++ a segunda usa herança PRIVADA — "é implementado em termos de", não
//  "é um" (ver src/oop/demo_inheritance.hpp, 1.1).
//
//  A recomendação moderna é quase sempre a primeira. A segunda está aqui para
//  mostrar por que ela perdeu.
// =============================================================================
#include "interface_moderna.hpp"
#include "legado.hpp"

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace adapter {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Adapter de OBJETO — composição
//
//  Além de traduzir a interface, ele resolve três problemas de uma vez:
//    • RAII: o handle é liberado no destrutor, sem `destroy` manual
//    • tradução de erro: LegacyResult → std::expected<void, ErroDeLog>
//    • tradução de tipo: Nivel → int, string_view → (char*, len)
// ─────────────────────────────────────────────────────────────────────────────
class LogAdaptado final : public Log {
public:
    explicit LogAdaptado(const std::string& nome)
        : handle_(legado::legacy_logger_create(nome.c_str())) {}

    // RAII: o `destroy` que o usuário da API C teria que lembrar de chamar
    ~LogAdaptado() override {
        if (handle_) legado::legacy_logger_destroy(handle_);
    }

    // Move-only: um handle tem um dono só
    LogAdaptado(const LogAdaptado&)            = delete;
    LogAdaptado& operator=(const LogAdaptado&) = delete;
    LogAdaptado(LogAdaptado&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }

    std::expected<void, ErroDeLog> escrever(Nivel n, std::string_view msg) override {
        const auto r = legado::legacy_logger_write(
            handle_, para_nivel_legado(n), msg.data(), msg.size());
        return traduzir(r);
    }

    std::expected<void, ErroDeLog> descarregar() override {
        return traduzir(legado::legacy_logger_flush(handle_));
    }

    std::size_t pendentes() const override {
        return legado::legacy_logger_count(handle_);
    }

private:
    static int para_nivel_legado(Nivel n) {
        switch (n) {
            case Nivel::Debug: return 0;
            case Nivel::Info:  return 1;
            case Nivel::Aviso: return 2;
            case Nivel::Erro:  return 3;
        }
        return 1;
    }

    static std::expected<void, ErroDeLog> traduzir(legado::LegacyResult r) {
        switch (r) {
            case legado::LEGACY_OK:          return {};
            case legado::LEGACY_NOT_OPEN:    return std::unexpected{ErroDeLog::NaoAberto};
            case legado::LEGACY_BUFFER_FULL: return std::unexpected{ErroDeLog::BufferCheio};
            case legado::LEGACY_BAD_LEVEL:   return std::unexpected{ErroDeLog::NivelInvalido};
        }
        return std::unexpected{ErroDeLog::Desconhecido};
    }

    legado::LegacyLogger* handle_;
};

}  // namespace adapter
