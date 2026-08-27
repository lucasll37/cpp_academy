// =============================================================================
//  src/linkage/widget.cpp
//
//  A implementação escondida do pimpl de demo_abi.hpp.
//
//  Este arquivo é a OUTRA metade do idioma: aqui `Impl` finalmente ganha uma
//  definição, e é por isso que o destrutor e o move só podem ser `= default`
//  AQUI, depois dela — nunca no cabeçalho.
// =============================================================================
#include "widget.hpp"

#include <string>
#include <utility>

namespace demo_abi {

// A definição que o cabeçalho nunca vê. Acrescentar membros aqui NÃO muda
// `sizeof(Widget)` e portanto não quebra a ABI.
struct Widget::Impl {
    std::string nome;
    int         acessos = 0;
};

Widget::Widget(std::string nome)
    : pimpl_{std::make_unique<Impl>(Impl{std::move(nome), 0})} {}

// `= default` AQUI, onde `Impl` é completa. No cabeçalho, o `unique_ptr`
// não conseguiria instanciar o seu deleter.
Widget::~Widget()                                    = default;
Widget::Widget(Widget&&) noexcept                    = default;
Widget& Widget::operator=(Widget&&) noexcept         = default;

std::string Widget::descricao() const {
    ++pimpl_->acessos;
    return pimpl_->nome + " (acessos: " + std::to_string(pimpl_->acessos) + ")";
}

void Widget::renomear(std::string novo) { pimpl_->nome = std::move(novo); }

}  // namespace demo_abi
