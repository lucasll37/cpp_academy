#pragma once
// =============================================================================
//  no.hpp — a interface UNIFORME entre folha e composto
//
//  A ideia central do Composite: o cliente trata um objeto ÚNICO e uma
//  COMPOSIÇÃO de objetos da mesma forma. Um arquivo e uma pasta respondem à
//  mesma pergunta "qual o seu tamanho?".
//
//  A decisão de design que o padrão força, e que o GoF discute explicitamente:
//
//    onde ficam `adicionar` e `remover`?
//      • na INTERFACE  → uniformidade máxima, mas uma Folha precisa
//                        implementar `adicionar` de alguma forma (lançar? ignorar?)
//                        — e isso viola o LSP (src/solid/lsp/)
//      • só no COMPOSTO → o cliente precisa saber o tipo para compor,
//                        mas a interface fica honesta
//
//  Escolhemos a SEGUNDA, e a seção 4 do main.cpp explica por quê.
// =============================================================================
#include <cstddef>
#include <string>

namespace composite {

class No {
public:
    virtual ~No() = default;

    // As operações que TODO nó responde — é aqui que mora a uniformidade
    virtual std::string nome() const = 0;
    virtual std::size_t tamanho() const = 0;
    virtual std::size_t contar_arquivos() const = 0;

    // ⚠ NVI (Non-Virtual Interface), e não é estilo: é correção.
    //
    //   A primeira versão deste arquivo tinha:
    //       virtual void imprimir(int nivel = 0) const = 0;
    //
    //   Argumento padrão em função VIRTUAL é uma armadilha conhecida: o corpo
    //   é escolhido dinamicamente, mas o default é escolhido ESTATICAMENTE,
    //   pelo tipo do ponteiro. Aqui o sintoma foi imediato — `raiz->imprimir()`
    //   num `Pasta*` nem compilou, porque o override não repetiu o default.
    //
    //   A correção é a que src/oop/demo_virtual.hpp, seção 2.6 recomenda:
    //   método público NÃO-virtual aplica o default e delega ao virtual.
    void imprimir() const { imprimir_em(0); }

    virtual void imprimir_em(int nivel) const = 0;

    // ⚠ `adicionar`/`remover` NÃO estão aqui. Ver a discussão em 4.
};

}  // namespace composite
