// =============================================================================
//  src/linkage/demo_extern_c.hpp
//
//  extern "C" e o name mangling
//  ────────────────────────────
//  Em C, o símbolo de `int soma(int,int)` é `soma`. Ponto.
//  Em C++, é `_Z4somaii` — porque o C++ precisa distinguir sobrecargas,
//  namespaces e instanciações de template, e o formato de objeto só tem
//  strings para trabalhar. Essa codificação é o NAME MANGLING.
//
//  `extern "C"` diz: "para esta declaração, use a convenção do C". Não muda
//  a linguagem em que o código é escrito — muda como o SÍMBOLO é nomeado e
//  como os argumentos são passados.
//
//  ⚠ Este demo chama funções de c_api.c, que é compilado pelo compilador C
//    de verdade (`cc`), não pelo `c++`. Os símbolos vêm de lá.
//
//  Tópicos:
//  • O que o mangling codifica
//  • extern "C": o que ele permite e o que ele proíbe
//  • O idioma do cabeçalho compartilhado
//  • Callback: o C chamando o C++
//  • ⚠ exceções não atravessam a fronteira
// =============================================================================
#pragma once
#include "c_api.h"
#include <fmt/core.h>
#include <string>
#include <typeinfo>

namespace demo_extern_c {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O mangling
// ─────────────────────────────────────────────────────────────────────────────
namespace exemplos {
int  f(int)            { return 1; }
int  f(double)         { return 2; }
int  f(int, int)       { return 3; }
extern "C" int f_c(int){ return 4; }     // ← linkage C
}

void demo_mangling() {
    fmt::print("\n── 4.1 Name mangling ──\n");

    fmt::print("      Três sobrecargas de `f` no namespace `exemplos`, e uma\n");
    fmt::print("      função com linkage C. Os símbolos REAIS deste binário,\n");
    fmt::print("      copiados de `nm ./dist/bin/linkage`:\n\n");
    fmt::print("        int f(int)         → _ZN13demo_extern_c8exemplos1fEi\n");
    fmt::print("        int f(double)      → _ZN13demo_extern_c8exemplos1fEd\n");
    fmt::print("        int f(int, int)    → _ZN13demo_extern_c8exemplos1fEii\n");
    fmt::print("        extern \"C\" f_c(int) → f_c                   ← nome CRU\n\n");
    fmt::print("      Lendo `_ZN13demo_extern_c8exemplos1fEii`:\n");
    fmt::print("        _Z            prefixo de nome manglado\n");
    fmt::print("        N ... E       nome aninhado (namespaces)\n");
    fmt::print("        13demo_extern_c   comprimento + nome\n");
    fmt::print("        8exemplos     comprimento + nome\n");
    fmt::print("        1f            comprimento + nome da função\n");
    fmt::print("        ii            os PARÂMETROS: int, int\n\n");

    fmt::print("      Os endereços provam que são funções distintas:\n");
    fmt::print("        &f(int)      = {}\n",
               reinterpret_cast<const void*>(
                   static_cast<int(*)(int)>(&exemplos::f)));
    fmt::print("        &f(double)   = {}\n",
               reinterpret_cast<const void*>(
                   static_cast<int(*)(double)>(&exemplos::f)));
    fmt::print("        &f(int,int)  = {}\n",
               reinterpret_cast<const void*>(
                   static_cast<int(*)(int,int)>(&exemplos::f)));

    fmt::print("\n      O mangling codifica: namespace, nome, e os TIPOS DOS\n");
    fmt::print("      PARÂMETROS. NÃO codifica o tipo de RETORNO — e é por isso\n");
    fmt::print("      que sobrecarregar só pelo retorno é proibido na linguagem.\n\n");

    fmt::print("      Para ler símbolos manglados:\n");
    fmt::print("        nm -C arq.o                  # nm já desmangla\n");
    fmt::print("        echo '_Z4somaii' | c++filt   # desmangla à mão\n");
    fmt::print("        abi::__cxa_demangle(...)     # em runtime, <cxxabi.h>\n\n");

    fmt::print("      ⚠  O esquema NÃO é padronizado. GCC e Clang usam o Itanium\n");
    fmt::print("         ABI; o MSVC usa outro completamente diferente. É por isso\n");
    fmt::print("         que um .lib do MSVC não linka com o MinGW.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Chamando C de verdade
// ─────────────────────────────────────────────────────────────────────────────
void demo_chamando_c() {
    fmt::print("\n── 4.2 Chamando C de verdade ──\n");

    fmt::print("      As funções abaixo estão em c_api.c, compilado por `cc`:\n\n");
    fmt::print("        soma_c(20, 22)            = {}\n", soma_c(20, 22));
    fmt::print("        nome_da_linguagem_c()     = \"{}\"\n", nome_da_linguagem_c());
    fmt::print("        tamanho_de_int_em_c()     = {} (e em C++: {})\n",
               tamanho_de_int_em_c(), sizeof(int));

    fmt::print("\n      Sem o `extern \"C\"` no c_api.h, este arquivo geraria uma\n");
    fmt::print("      chamada para `_Z6soma_cii`, e o linker responderia:\n");
    fmt::print("        undefined reference to `soma_c(int, int)'\n");
    fmt::print("      — enquanto `nm c_api.o` mostraria, ali, um `T soma_c`.\n");
    fmt::print("      É o erro de integração C/C++ mais comum que existe.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Callback: o C chamando o C++
// ─────────────────────────────────────────────────────────────────────────────
extern "C" int dobra(int v) { return v * 2; }   // ← precisa de linkage C

void demo_callback() {
    fmt::print("\n── 4.3 Callback: C chamando C++ ──\n");

    fmt::print("      `aplica_c` está em C e recebe um ponteiro de função:\n");
    fmt::print("        typedef int (*transformador_t)(int);\n");
    fmt::print("        int aplica_c(transformador_t f, int v);\n\n");

    fmt::print("      aplica_c(dobra, 21) = {}\n", aplica_c(&dobra, 21));

    fmt::print("\n      `dobra` é declarada `extern \"C\"` porque, a rigor, o tipo\n");
    fmt::print("      de um ponteiro para função COM linkage C é diferente do\n");
    fmt::print("      tipo de um ponteiro para função com linkage C++. Na prática\n");
    fmt::print("      GCC e Clang aceitam os dois, mas o padrão não garante e\n");
    fmt::print("      alguns compiladores recusam.\n\n");

    fmt::print("      ⚠  Uma LAMBDA sem captura converte para ponteiro de função,\n");
    fmt::print("         mas o resultado tem linkage C++. Para APIs de C que\n");
    fmt::print("         exigem rigor, use uma função `extern \"C\"` nomeada.\n\n");

    fmt::print("      ⚠  Uma lambda COM captura NÃO converte para ponteiro de\n");
    fmt::print("         função — não há onde guardar o estado. O idioma para\n");
    fmt::print("         isso é o parâmetro `void* user_data` que quase toda API\n");
    fmt::print("         de C oferece:\n");
    fmt::print("           extern \"C\" void trampolim(void* ud) {{\n");
    fmt::print("               (*static_cast<std::function<void()>*>(ud))();\n");
    fmt::print("           }}\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ O que extern "C" proíbe
// ─────────────────────────────────────────────────────────────────────────────
void demo_limites() {
    fmt::print("\n── 4.4 ⚠ Os limites ──\n");

    fmt::print("      `extern \"C\"` NÃO permite:\n");
    fmt::print("        • sobrecarga — só há UM símbolo `soma`, não dois;\n");
    fmt::print("        • templates com linkage C;\n");
    fmt::print("        • funções membro não-estáticas.\n\n");

    fmt::print("      `extern \"C\"` PERMITE:\n");
    fmt::print("        • usar C++ DENTRO do corpo — classes, exceções, STL;\n");
    fmt::print("        • sobrecarga em torno: `extern \"C\"` numa e C++ nas outras;\n");
    fmt::print("        • tipos de parâmetro que são classes C++ (embora só faça\n");
    fmt::print("          sentido se os dois lados forem C++).\n\n");

    fmt::print("      ⚠  EXCEÇÕES NÃO ATRAVESSAM.\n");
    fmt::print("         Uma exceção que escapa de uma função `extern \"C\"` chamada\n");
    fmt::print("         a partir de C é comportamento indefinido: as frames de C\n");
    fmt::print("         normalmente não têm informação de unwinding, e os\n");
    fmt::print("         destrutores intermediários não rodam.\n\n");
    fmt::print("         A fronteira precisa ser à prova:\n");
    fmt::print("           extern \"C\" int minha_api(int x) noexcept {{\n");
    fmt::print("               try {{ return impl(x); }}\n");
    fmt::print("               catch (const std::exception& e) {{\n");
    fmt::print("                   guarda_erro(e.what()); return -1;\n");
    fmt::print("               }} catch (...) {{ return -2; }}\n");
    fmt::print("           }}\n");
    fmt::print("         Erros viram CÓDIGOS DE RETORNO na fronteira — é o que\n");
    fmt::print("         `std::error_code` faz em src/errors/, e o motivo de toda\n");
    fmt::print("         API de C ser assim.\n\n");

    fmt::print("      ⚠  A outra fronteira que vaza: um objeto alocado com `new`\n");
    fmt::print("         no C++ NÃO pode ser liberado com `free` no C. Exporte\n");
    fmt::print("         SEMPRE um `minha_api_free(ptr)` junto do que aloca.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_mangling();
    demo_chamando_c();
    demo_callback();
    demo_limites();
}

}  // namespace demo_extern_c
