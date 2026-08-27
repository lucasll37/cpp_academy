// =============================================================================
//  src/linkage/widget.hpp
//
//  A metade pública do pimpl — e uma lição aprendida na marra
//  ──────────────────────────────────────────────────────────
//  Este arquivo existe por um motivo concreto. A primeira versão do
//  subprojeto declarava `Widget` dentro de `demo_abi.hpp`, e `widget.cpp`
//  incluía `demo_abi.hpp` para alcançá-la. O linker respondeu:
//
//      multiple definition of `demo_abi::demo_o_que_e()'
//      first defined here
//
//  As funções `demo_*()` daquele cabeçalho não são `inline`. Com UMA TU
//  incluindo, tudo bem; com duas, cada uma define os mesmos símbolos e a
//  ODR é violada — exatamente o caso que demo_traducao.hpp descreve em 1.4.
//
//  A correção é a que todo projeto acaba adotando: um cabeçalho pequeno,
//  com só o que precisa ser compartilhado entre TUs, e sem nada definido.
// =============================================================================
#pragma once
#include <memory>
#include <string>

namespace demo_abi {

class Widget {
    struct Impl;                                  // só DECLARADA aqui
    std::unique_ptr<Impl> pimpl_;

public:
    explicit Widget(std::string nome);

    // ⚠ Estes quatro são DECLARADOS aqui e definidos em widget.cpp. Não podem
    //   ser `= default` no cabeçalho: `unique_ptr<Impl>` precisa do tipo
    //   COMPLETO para instanciar o seu deleter, e aqui `Impl` é incompleta.
    ~Widget();
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;

    std::string descricao() const;
    void        renomear(std::string novo);
};

}  // namespace demo_abi
