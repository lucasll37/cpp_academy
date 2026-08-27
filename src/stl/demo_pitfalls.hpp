// =============================================================================
//  src/stl/demo_pitfalls.hpp
//
//  Armadilhas da STL — o que compila, roda, e está errado
//  ──────────────────────────────────────────────────────
//  Esta é a parte da STL que ninguém ensina e todo mundo tropeça. Nenhum dos
//  casos abaixo gera erro de compilação. Alguns nem geram warning. Todos
//  produzem comportamento que o autor não esperava.
//
//  Tópicos:
//  • std::remove não remove nada
//  • vector<bool> não é um container de bool
//  • map::operator[] insere silenciosamente
//  • Referência para dentro de container que realocou
//  • string_view pendurado
//  • A ordem de avaliação e a invalidação no mesmo statement
//  • Comparar iteradores de containers diferentes
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace demo_pitfalls {

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::remove NÃO remove
//
//     Um algoritmo só enxerga ITERADORES — ele não tem acesso ao container e
//     portanto não pode mudar o tamanho dele. `std::remove` empurra os
//     elementos que ficam para o início e devolve a nova fronteira lógica.
//     O que sobra depois da fronteira é lixo (elementos movidos).
// ─────────────────────────────────────────────────────────────────────────────
void demo_remove() {
    fmt::print("\n── 5.1 std::remove não remove ──\n");

    std::vector<int> v{1, 2, 3, 2, 4, 2, 5};
    fmt::print("      original:              {}  (size={})\n", fmt::join(v, " "), v.size());

    // ✗ O erro: chamar remove e achar que acabou
    auto copia = v;
    auto nova_fronteira = std::remove(copia.begin(), copia.end(), 2);
    fmt::print("      após std::remove(2):   {}  (size={})  ← size NÃO mudou!\n",
               fmt::join(copia, " "), copia.size());
    fmt::print("      elementos válidos:     {}\n",
               fmt::join(std::vector<int>(copia.begin(), nova_fronteira), " "));

    // ✓ O idioma erase-remove: o algoritmo reorganiza, o container encurta
    std::vector<int> w{1, 2, 3, 2, 4, 2, 5};
    w.erase(std::remove(w.begin(), w.end(), 2), w.end());
    fmt::print("      erase-remove:          {}  (size={})\n", fmt::join(w, " "), w.size());

    // ✓ C++20: std::erase / std::erase_if fazem os dois passos
    std::vector<int> z{1, 2, 3, 2, 4, 2, 5};
    std::erase(z, 2);
    fmt::print("      std::erase (C++20):    {}  (size={})\n", fmt::join(z, " "), z.size());

    fmt::print("      Vale o mesmo para unique, remove_if e todo algoritmo que\n");
    fmt::print("      \"remove\": eles reorganizam, quem apaga é o container.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. vector<bool> não é um vector de bool
//
//     É uma especialização que empacota 8 bools por byte. Economiza memória e
//     quebra o contrato de container: operator[] devolve um PROXY, não um bool&.
// ─────────────────────────────────────────────────────────────────────────────
void demo_vector_bool() {
    fmt::print("\n── 5.2 vector<bool> ──\n");

    std::vector<bool> vb{true, false, true};
    std::vector<char> vc{1, 0, 1};

    fmt::print("      sizeof(vector<bool>) armazena 8 valores por byte\n");

    // `auto` deduz o PROXY, não bool. Guardá-lo além da vida do vector é UB.
    auto proxy = vb[0];
    fmt::print("      auto x = vb[0]  → tipo é std::_Bit_reference, não bool\n");
    fmt::print("      é bool? {}\n", std::is_same_v<decltype(proxy), bool>);
    fmt::print("      já em vector<char>, auto y = vc[0] → é char? {}\n",
               std::is_same_v<decltype(vc[0]), char&>);

    // bool& não liga no proxy — isto NÃO compila:
    // bool& r = vb[0];             // ✗ erro

    // E o proxy tem semântica de referência: mudar a cópia muda o vector!
    proxy = false;
    fmt::print("      após `proxy = false`: vb[0] = {}  ← a \"cópia\" era uma referência\n",
               static_cast<bool>(vb[0]));

    fmt::print("      Alternativas: std::vector<char>, std::deque<bool>,\n");
    fmt::print("      std::bitset<N> (tamanho fixo) ou boost::dynamic_bitset.\n");
    fmt::print("      Se precisa mesmo de vector<bool>, use `bool x = vb[i];`\n");
    fmt::print("      explicitamente — nunca `auto`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. map::operator[] insere
//
//     `m[k]` significa "me dê uma referência para o valor de k, CRIANDO uma
//     entrada default se não existir". Consultar com ele engorda o mapa.
// ─────────────────────────────────────────────────────────────────────────────
void demo_map_bracket() {
    fmt::print("\n── 5.3 map::operator[] insere ──\n");

    std::map<std::string, int> estoque{{"maçã", 5}, {"pêra", 3}};
    fmt::print("      inicial: {} itens\n", estoque.size());

    // ✗ "só consultando"...
    if (estoque["banana"] > 0) fmt::print("      tem banana\n");
    fmt::print("      após consultar \"banana\": {} itens ← inseriu {{\"banana\", 0}}!\n",
               estoque.size());

    // ✓ As formas de consultar sem inserir:
    fmt::print("      count(\"uva\")  → {}  (não insere)\n", estoque.count("uva"));
    fmt::print("      contains(\"uva\")→ {}  (C++20, não insere)\n", estoque.contains("uva"));

    if (auto it = estoque.find("uva"); it != estoque.end())
        fmt::print("      find achou\n");
    else
        fmt::print("      find(\"uva\")   → end() (não insere)\n");

    fmt::print("      itens ao final: {}\n", estoque.size());

    fmt::print("      Consequência prática: `operator[]` não existe em map CONST.\n");
    fmt::print("      Se o seu código não compila num map const, é este o motivo.\n");
    fmt::print("      Use `.at(k)` (lança out_of_range) ou `.find(k)`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Referência para dentro de um container que cresceu
//
//     Este é o bug de STL número 1. A referência é obtida, o container realoca,
//     e a referência passa a apontar para memória liberada.
// ─────────────────────────────────────────────────────────────────────────────
void demo_referencia_invalidada() {
    fmt::print("\n── 5.4 Referência sobrevivendo à realocação ──\n");

    std::vector<int> v{1, 2, 3};
    fmt::print("      v = {}  capacity={}\n", fmt::join(v, " "), v.capacity());

    // ✗ O padrão perigoso:
    //     int& primeiro = v[0];
    //     v.push_back(4);          // realoca
    //     primeiro = 99;           // ← escrita em memória liberada (UB)
    fmt::print("      int& primeiro = v[0];\n");
    fmt::print("      v.push_back(4);   // realoca — `primeiro` fica pendurado\n");
    fmt::print("      primeiro = 99;    // ← heap-use-after-free\n");
    fmt::print("      As 3 linhas estão comentadas: o ASan aborta o programa.\n");

    // ✓ A correção: guarde o ÍNDICE, não a referência
    std::size_t indice = 0;
    v.push_back(4);
    v[indice] = 99;
    fmt::print("      correção (guardar o índice): v = {}\n", fmt::join(v, " "));

    fmt::print("      Um caso especialmente traiçoeiro:\n");
    fmt::print("        v.push_back(v[0]);   // o argumento pode ser invalidado\n");
    fmt::print("        pela realocação ANTES de ser copiado. A STL trata este\n");
    fmt::print("        caso específico, mas o seu container caseiro não trataria.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. string_view pendurado
//
//     `string_view` NÃO POSSUI os caracteres. Ele é um par (ponteiro, tamanho).
//     Se a string dona morrer, o view aponta para o nada.
// ─────────────────────────────────────────────────────────────────────────────
std::string produz_string() { return "sou um temporário"; }

void demo_string_view() {
    fmt::print("\n── 5.5 string_view pendurado ──\n");

    // ✗ O erro clássico: view de um temporário
    //     std::string_view sv = produz_string();
    //     // o temporário morre AQUI, no fim do statement
    //     fmt::print("{}", sv);   // ← UB
    fmt::print("      std::string_view sv = produz_string();\n");
    fmt::print("      ↑ o temporário morre no fim do statement; `sv` fica pendurado.\n");
    fmt::print("        (comentado: o ASan detectaria stack/heap-use-after-scope)\n");

    // ✓ Manter a dona viva
    std::string dona = produz_string();
    std::string_view sv = dona;
    fmt::print("      correção: guardar a std::string → '{}'\n", sv);

    // ✗ Outro caso: view de um membro de objeto que sai de escopo
    fmt::print("      Vale para qualquer view: span, ranges::view, iteradores.\n");
    fmt::print("      Regra: um view é PARÂMETRO, não MEMBRO nem RETORNO.\n");

    // E cuidado com a terminação: string_view não é null-terminated
    std::string texto = "abcdef";
    std::string_view parcial = std::string_view(texto).substr(0, 3);
    fmt::print("      substr(0,3) = '{}' — mas NÃO tem '\\0' no fim.\n", parcial);
    fmt::print("      Passar `parcial.data()` para uma API C leria além do fim.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Comparar iteradores de containers diferentes
//
//     `it1 != it2` com iteradores de containers distintos é UB — não é `false`,
//     é indefinido. Loops que misturam dois containers costumam esconder isto.
// ─────────────────────────────────────────────────────────────────────────────
void demo_iteradores_incompativeis() {
    fmt::print("\n── 5.6 Iteradores de containers diferentes ──\n");

    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 2, 3};

    fmt::print("      a e b têm o mesmo conteúdo, mas:\n");
    fmt::print("        a.begin() != b.begin()  → comparar isto é UB\n");
    fmt::print("        std::copy(a.begin(), b.end(), ...)  → UB\n");
    fmt::print("      Compile com -D_GLIBCXX_DEBUG para o libstdc++ detectar:\n");
    fmt::print("        \"attempt to compare iterators from different sequences\"\n");

    // ✓ Comparar CONTEÚDO é outra coisa, e é bem definido
    fmt::print("      std::equal(a,b) → {}  (esta é a comparação que você queria)\n",
               std::equal(a.begin(), a.end(), b.begin(), b.end()));
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. O resumo
// ─────────────────────────────────────────────────────────────────────────────
void demo_resumo() {
    fmt::print("\n── 5.7 Resumo das armadilhas ──\n");

    fmt::print("      | armadilha                  | correção                     |\n");
    fmt::print("      |----------------------------|------------------------------|\n");
    fmt::print("      | std::remove não remove     | erase-remove ou std::erase_if|\n");
    fmt::print("      | vector<bool> devolve proxy | vector<char> / bitset        |\n");
    fmt::print("      | map[k] insere ao consultar | .find / .at / .contains      |\n");
    fmt::print("      | referência após realocação | guarde o índice              |\n");
    fmt::print("      | string_view de temporário  | view é parâmetro, não membro |\n");
    fmt::print("      | iteradores de origens ≠    | -D_GLIBCXX_DEBUG             |\n");
    fmt::print("      | comparador com <=          | use < (strict weak ordering) |\n");

    fmt::print("\n      Duas ferramentas que pegam quase tudo isto:\n");
    fmt::print("        g++ -fsanitize=address,undefined   (em runtime)\n");
    fmt::print("        g++ -D_GLIBCXX_DEBUG               (checagens da libstdc++)\n");
    fmt::print("      Rode os testes sob as duas pelo menos uma vez.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_remove();
    demo_vector_bool();
    demo_map_bracket();
    demo_referencia_invalidada();
    demo_string_view();
    demo_iteradores_incompativeis();
    demo_resumo();
}

} // namespace demo_pitfalls
