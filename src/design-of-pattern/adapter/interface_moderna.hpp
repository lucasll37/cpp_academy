#pragma once
// =============================================================================
//  interface_moderna.hpp — o que o SEU sistema espera
//
//  Esta é a interface que o resto do código já usa. Ela foi desenhada sem
//  saber que a biblioteca legada existe — e é assim que deve ser: a interface
//  pertence ao CLIENTE (ver src/solid/isp/, seção 2).
// =============================================================================
#include <expected>
#include <string>
#include <string_view>

namespace adapter {

enum class Nivel { Debug, Info, Aviso, Erro };

enum class ErroDeLog {
    NaoAberto,
    BufferCheio,
    NivelInvalido,
    Desconhecido,
};

std::string_view descrever(ErroDeLog e);

// A interface moderna: std::expected, string_view, enum class, RAII
class Log {
public:
    virtual ~Log() = default;

    virtual std::expected<void, ErroDeLog> escrever(Nivel n, std::string_view msg) = 0;
    virtual std::expected<void, ErroDeLog> descarregar() = 0;
    virtual std::size_t pendentes() const = 0;

    // Conveniências que a interface base dá de graça
    auto debug(std::string_view m) { return escrever(Nivel::Debug, m); }
    auto info(std::string_view m)  { return escrever(Nivel::Info,  m); }
    auto aviso(std::string_view m) { return escrever(Nivel::Aviso, m); }
    auto erro(std::string_view m)  { return escrever(Nivel::Erro,  m); }
};

}  // namespace adapter
