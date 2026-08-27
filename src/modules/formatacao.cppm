// =============================================================================
//  src/modules/formatacao.cppm — um módulo que IMPORTA outro
//
//  Demonstra:
//    • `import` entre módulos
//    • `export import` — reexportar (o consumidor vê os dois)
//    • que o grafo de dependências é explícito e verificado pelo compilador
// =============================================================================
module;
#include <string>

export module formatacao;

// `export import` faz as DUAS coisas: importa para uso interno E reexporta,
// de modo que quem importar `formatacao` também enxergue `geometria`.
// A ORDEM de compilação passa a importar: o `geometria.gcm` precisa existir
// antes de este arquivo compilar.
export import geometria;

export namespace formatacao {

std::string descrever(const geometria::Ponto& p) {
    return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
}

std::string descrever_distancia(const geometria::Ponto& a, const geometria::Ponto& b) {
    return descrever(a) + " → " + descrever(b) + " = " +
           std::to_string(geometria::distancia(a, b));
}

}  // namespace formatacao
