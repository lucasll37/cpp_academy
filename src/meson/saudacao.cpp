// =============================================================================
//  src/meson/saudacao.cpp
//  Implementação da biblioteca — compilada UMA vez, linkada por dois alvos
//  (o executável `meson_demo` e o teste `test_saudacao`).
// =============================================================================
#include "saudacao.hpp"

#include <fmt/core.h>

namespace saudacao {

std::string cumprimentar(const std::string& nome) {
    return fmt::format("Olá, {}! Esta linha veio de libsaudacao.a", nome);
}

std::size_t tamanho_da_saudacao(const std::string& nome) {
    return cumprimentar(nome).size();
}

} // namespace saudacao
