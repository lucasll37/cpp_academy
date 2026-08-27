// =============================================================================
//  src/linkage/unidade_b.cpp
//
//  Uma das duas unidades de tradução do experimento. Este arquivo e
//  unidade_a.cpp são IDÊNTICOS a menos do nome — o que muda é só o
//  fato de serem compilados separadamente e linkados juntos.
// =============================================================================
#include "comum.hpp"

namespace comum {

Enderecos enderecos_de_b() {
    return Enderecos{
        "unidade_b.cpp",
        reinterpret_cast<const void*>(&funcao_inline),
        reinterpret_cast<const void*>(&funcao_static),
        reinterpret_cast<const void*>(&funcao_anonima),
        static_cast<const void*>(&contador_inline),
        static_cast<const void*>(&contador_static),
        static_cast<const void*>(&kConstante),
        static_cast<const void*>(&kConstexprInline),
        reinterpret_cast<const void*>(&identidade<int>),
    };
}

void incrementa_em_b(int n) {
    for (int i = 0; i < n; ++i) { ++contador_inline; ++contador_static; }
}

int contador_inline_visto_de_b() { return contador_inline; }
int contador_static_visto_de_b() { return contador_static; }

}  // namespace comum
