// =============================================================================
//  src/meson/saudacao.hpp
//
//  A parte "biblioteca" do exemplo. Existe para que o meson.build tenha algo
//  que não seja um executável: uma static_library, empacotada num
//  `declare_dependency()` e consumida pelo executável e pelo teste.
//
//  Repare que este header é instalado (`install_headers`) enquanto o .cpp vira
//  um .a que NÃO é instalado — é a divisão normal entre interface e
//  implementação, e o meson.build reflete isso em duas linhas.
// =============================================================================
#pragma once
#include <string>

namespace saudacao {

/// Monta a saudação. Vive na biblioteca, não no executável.
std::string cumprimentar(const std::string& nome);

/// Quantos caracteres a saudação terá — usado pelo teste para provar que o
/// binário de teste está mesmo linkando a biblioteca, e não recompilando o .cpp.
std::size_t tamanho_da_saudacao(const std::string& nome);

} // namespace saudacao
