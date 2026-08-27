#include "tema_claro.hpp"
#include "tema_escuro.hpp"

#include <fmt/core.h>

namespace abstract_factory::claro {

std::string Botao::desenhar() const  { return fmt::format("[ {} ]", rotulo_); }
std::string Botao::ao_clicar() const { return "brilho suave"; }

std::string Campo::desenhar(const std::string& c) const {
    return fmt::format("|{:_<20}|", c);
}

std::string Caixa::desenhar(bool m) const { return m ? "[x]" : "[ ]"; }

}  // namespace abstract_factory::claro

namespace abstract_factory::escuro {

std::string Botao::desenhar() const  { return fmt::format("▐ {} ▌", rotulo_); }
std::string Botao::ao_clicar() const { return "pulso neon"; }

std::string Campo::desenhar(const std::string& c) const {
    return fmt::format("▕{:·<20}▏", c);
}

std::string Caixa::desenhar(bool m) const { return m ? "◼" : "◻"; }

}  // namespace abstract_factory::escuro
