// =============================================================================
//  src/modules/main.cpp — o consumidor
//
//  ⚠ ESTE SUBPROJETO É DIFERENTE DE TODOS OS OUTROS.
//
//  Módulos exigem uma ORDEM DE COMPILAÇÃO: cada interface precisa ser compilada
//  antes de quem a importa, e o artefato binário (`.gcm`) precisa estar no
//  cache quando o consumidor compila. Isso é o oposto dos headers, em que a
//  ordem não importa porque tudo é reprocessado do zero em cada TU.
//
//  Por isso este subprojeto tem um Makefile próprio, e o README explica por que
//  ele não está no build do Meson.
// =============================================================================

// `import` substitui `#include`. Repare que NÃO há `#include <cmath>` aqui,
// embora `geometria` use `std::sqrt` internamente: o módulo não vaza os
// símbolos dos headers que ele incluiu.
import formatacao;             // e, por `export import`, também `geometria`

#include <cstdio>              // headers antigos ainda funcionam, misturados

// ─────────────────────────────────────────────────────────────────────────────
//  Um cabeçalho simples, sem depender de fmt — este alvo é construído fora do
//  Meson, então não temos as flags do Conan aqui.
// ─────────────────────────────────────────────────────────────────────────────
static void secao(const char* titulo) {
    std::printf("\n╔══════════════════════════════════════════════════════╗\n");
    std::printf("║  %-50s  ║\n", titulo);
    std::printf("╚══════════════════════════════════════════════════════╝\n");
}

int main() {
    secao("1 · Usando um modulo importado");

    const geometria::Ponto a{0.0, 0.0};
    const geometria::Ponto b{3.0, 4.0};

    std::printf("      geometria::distancia({0,0}, {3,4}) = %.2f\n",
                geometria::distancia(a, b));
    std::printf("      geometria::area_circulo(2.0)         = %.4f\n",
                geometria::area_circulo(2.0));
    std::printf("      geometria::PI                        = %.10f\n", geometria::PI);

    std::printf("\n      templates funcionam em modulos:\n");
    std::printf("        maior(3, 7)       = %d\n", geometria::maior(3, 7));
    std::printf("        maior(2.5, 1.5)   = %.1f\n", geometria::maior(2.5, 1.5));

    secao("2 · export import — reexportacao");

    std::printf("      main.cpp fez apenas `import formatacao;`\n");
    std::printf("      mas enxerga `geometria` tambem, porque `formatacao`\n");
    std::printf("      declarou `export import geometria;`\n\n");
    std::printf("      %s\n", formatacao::descrever_distancia(a, b).c_str());

    secao("3 · O que NAO vaza");

    std::printf("      `geometria` faz #include <cmath> no global module\n");
    std::printf("      fragment, e usa std::sqrt internamente.\n");
    std::printf("      Este main.cpp NAO inclui <cmath>.\n\n");
    std::printf("      Se voce descomentar a linha abaixo em main.cpp:\n");
    std::printf("        // double r = std::sqrt(16.0);\n");
    std::printf("      o resultado e:\n");
    std::printf("        error: 'sqrt' is not a member of 'std'\n\n");
    std::printf("      Com um HEADER, `std::sqrt` estaria disponivel aqui sem\n");
    std::printf("      voce ter pedido — e essa poluicao e' cumulativa: um\n");
    std::printf("      header que inclui outro que inclui outro acaba trazendo\n");
    std::printf("      milhares de nomes que ninguem pediu.\n");

    // double r = std::sqrt(16.0);   // ← descomente para ver o erro

    std::printf("\n      O mesmo vale para `detalhe::quadrado`, definida em\n");
    std::printf("      geometria.cppm sem `export`: ela e' invisivel aqui.\n");
    std::printf("        // detalhe::quadrado(2.0);  →  'detalhe' has not been declared\n");

    secao("4 · O que isso custa e o que economiza");

    std::printf("      COMPILACAO:\n");
    std::printf("        header : o texto e' reprocessado em CADA .cpp que inclui\n");
    std::printf("        modulo : compilado UMA vez; consumidores leem o .gcm\n\n");
    std::printf("      ORDEM:\n");
    std::printf("        header : irrelevante — cada TU e' independente\n");
    std::printf("        modulo : OBRIGATORIA — a interface antes do consumidor\n\n");
    std::printf("      ISOLAMENTO:\n");
    std::printf("        header : macros e nomes vazam nos dois sentidos\n");
    std::printf("        modulo : so' o que tem `export` atravessa\n\n");
    std::printf("      MACROS:\n");
    std::printf("        header : #define do incluidor afeta o incluido\n");
    std::printf("        modulo : macros NAO atravessam o import\n");

    std::printf("\n✓ Todos os demos de modulos concluidos!\n\n");
    return 0;
}
