// =============================================================================
//  src/type_erasure/demo_any.hpp
//
//  O apagamento de tipo que já está na biblioteca padrão
//  ─────────────────────────────────────────────────────
//  `std::function` apaga o tipo preservando UMA operação: chamar. Os outros
//  apagadores do padrão preservam conjuntos diferentes:
//
//    std::any               → só destruir e copiar (nem imprimir dá)
//    std::shared_ptr<void>  → só DESTRUIR — mas destruir CERTO
//    std::move_only_function→ chamar, sem exigir cópia   (C++23)
//    std::type_index        → identidade de tipo em runtime
//
//  Tópicos:
//  • std::any e o custo do any_cast
//  • ⚠ o deleter apagado do shared_ptr — o detalhe que unique_ptr não tem
//  • move_only_function (C++23)
//  • type_index: mapa de tipo → comportamento
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <any>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

namespace demo_any {

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::any
// ─────────────────────────────────────────────────────────────────────────────
void demo_std_any() {
    fmt::print("\n── 3.1 std::any ──\n");

    std::vector<std::any> caixa;
    caixa.emplace_back(42);
    caixa.emplace_back(3.14);
    caixa.emplace_back(std::string{"texto"});
    caixa.emplace_back(std::vector<int>{1, 2, 3});

    fmt::print("      tipos guardados (via type().name(), manglado):\n");
    for (const auto& a : caixa)
        fmt::print("        {}\n", a.type().name());

    fmt::print("\n      recuperar exige saber o tipo EXATO:\n");
    fmt::print("        any_cast<int>(caixa[0]) = {}\n", std::any_cast<int>(caixa[0]));

    // any_cast<T&> lança; any_cast<T*> devolve nullptr
    try {
        (void)std::any_cast<long>(caixa[0]);          // int ≠ long: não converte
    } catch (const std::bad_any_cast& e) {
        fmt::print("        any_cast<long> em um int → lançou {}\n", e.what());
    }
    fmt::print("        any_cast<long>(&caixa[0]) = {}  (a forma de ponteiro não lança)\n",
               static_cast<const void*>(std::any_cast<long>(&caixa[0])));

    fmt::print("\n      ⚠  `any` NÃO faz conversões. `int` e `long` são tipos\n");
    fmt::print("         diferentes, ponto final. A comparação é `typeid` exato.\n\n");

    fmt::print("      sizeof(std::any) = {} bytes — também tem SBO.\n", sizeof(std::any));
    fmt::print("      `has_value()` e `reset()` completam a interface. É só isso:\n");
    fmt::print("      não dá para imprimir, comparar nem somar um `any` sem antes\n");
    fmt::print("      recuperar o tipo concreto.\n\n");

    fmt::print("      Quando usar: quase nunca. Antes de recorrer a `any`, veja se\n");
    fmt::print("        • um `std::variant` serve (lista fechada, mas com `visit`)\n");
    fmt::print("          → src/vocabulary_types/demo_variant.hpp\n");
    fmt::print("        • um apagamento SOB MEDIDA serve (a seção 2), preservando\n");
    fmt::print("          as operações que você de fato precisa.\n");
    fmt::print("      `any` é o caso em que você precisa guardar algo e devolvê-lo\n");
    fmt::print("      intacto sem NUNCA olhar dentro — plugin, blob de config, uma\n");
    fmt::print("      \"user data\" repassada a um callback.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ O deleter apagado do shared_ptr
//
//     Este é o exemplo de type erasure mais usado e menos percebido.
// ─────────────────────────────────────────────────────────────────────────────
void demo_shared_deleter() {
    fmt::print("\n── 3.2 ⚠ O deleter apagado do shared_ptr ──\n");

    fmt::print("      Compare as duas assinaturas:\n\n");
    fmt::print("        std::unique_ptr<T, D>   ← o deleter está NO TIPO\n");
    fmt::print("        std::shared_ptr<T>      ← e aqui, cadê?\n\n");

    fmt::print("      sizeof(unique_ptr<int>)              = {}\n",
               sizeof(std::unique_ptr<int>));
    {
        auto d = [](int* p) { delete p; };
        fmt::print("        com deleter lambda (sem estado)    = {}\n",
                   sizeof(std::unique_ptr<int, decltype(d)>));
    }
    fmt::print("      sizeof(shared_ptr<int>)              = {}  (ptr + ctrl block)\n",
               sizeof(std::shared_ptr<int>));

    fmt::print("\n      O `shared_ptr` guarda o deleter no BLOCO DE CONTROLE, com o\n");
    fmt::print("      tipo apagado. Consequências que valem ouro:\n\n");

    // (a) shared_ptr<Base> destrói corretamente MESMO SEM destrutor virtual
    struct Base { int a = 1; };                      // ← SEM virtual ~Base()

    static int destruidos = 0;
    struct Contada : Base { ~Contada() { ++destruidos; } };

    {
        std::shared_ptr<Base> p = std::make_shared<Contada>();
        // O deleter foi capturado quando o tipo AINDA era `Contada`.
    }
    fmt::print("      (a) shared_ptr<Base> apontando para Contada, SEM destrutor\n");
    fmt::print("          virtual na Base: ~Contada rodou? {} ({} vez)\n",
               destruidos == 1, destruidos);
    fmt::print("          O mesmo com `unique_ptr<Base>` seria UB — ele chamaria\n");
    fmt::print("          `delete static_cast<Base*>(p)`. Ver src/oop/demo_virtual.hpp.\n");
    fmt::print("          ⚠  isto NÃO é licença para omitir o destrutor virtual:\n");
    fmt::print("             só funciona se o shared_ptr foi criado do tipo derivado.\n\n");

    // (b) recurso de C com deleter customizado
    fmt::print("      (b) o mesmo mecanismo dá recursos de C sem esforço:\n");
    fmt::print("            std::shared_ptr<FILE> f{{fopen(...), &fclose}};\n");
    fmt::print("          O tipo continua sendo `shared_ptr<FILE>` — o `fclose`\n");
    fmt::print("          sumiu do tipo, mas não do comportamento.\n\n");

    fmt::print("      (c) o aliasing constructor guarda um ponteiro e mantém vivo\n");
    fmt::print("          OUTRO objeto — apagamento de tipo levado ao limite:\n");
    {
        struct Composto { int  parte = 7; };
        auto todo  = std::make_shared<Composto>();
        std::shared_ptr<int> pedaco{todo, &todo->parte};   // aponta para o membro
        fmt::print("          shared_ptr<int> para um membro: valor {}, use_count {}\n",
                   *pedaco, pedaco.use_count());
    }

    fmt::print("\n      O preço: o bloco de controle é uma alocação (evitável com\n");
    fmt::print("      `make_shared`, que funde as duas) e um nível de indireção\n");
    fmt::print("      a mais na destruição.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. move_only_function (C++23)
// ─────────────────────────────────────────────────────────────────────────────
void demo_move_only() {
    fmt::print("\n── 3.3 std::move_only_function (C++23) ──\n");

#if defined(__cpp_lib_move_only_function)
    auto recurso = std::make_unique<int>(42);

    // Uma lambda que captura por move é MOVE-ONLY. `std::function` a rejeita.
    std::move_only_function<int()> f = [r = std::move(recurso)] { return *r; };
    fmt::print("      lambda com unique_ptr capturado: f() = {}\n", f());

    auto g = std::move(f);                          // move: ok
    fmt::print("      após mover para g:               g() = {}\n", g());
    fmt::print("      f ainda é invocável? {}\n", static_cast<bool>(f));

    fmt::print("\n      A mesma lambda em `std::function` NÃO compila:\n");
    fmt::print("        error: static assertion failed:\n");
    fmt::print("        std::function target must be copy-constructible\n\n");

    fmt::print("      Ela também acertou os qualificadores da assinatura:\n");
    fmt::print("        move_only_function<int() const>   → operator() é const\n");
    fmt::print("        move_only_function<int() &&>      → só invocável em rvalue\n");
    fmt::print("        move_only_function<int() noexcept>→ propaga o noexcept\n");
    fmt::print("      `std::function` faz um const_cast interno (como a nossa v1\n");
    fmt::print("      da seção 2) e mente sobre const-correção.\n");
#else
    fmt::print("      Indisponível neste toolchain (__cpp_lib_move_only_function\n");
    fmt::print("      não definido). Precisa de GCC 12+ com -std=c++23.\n");
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. type_index — identidade de tipo como chave
// ─────────────────────────────────────────────────────────────────────────────
void demo_type_index() {
    fmt::print("\n── 3.4 std::type_index ──\n");

    // `std::type_info` não é copiável nem tem operator<. `type_index` é o
    // wrapper que o torna utilizável como chave de map.
    std::map<std::type_index, std::function<std::string(const std::any&)>> como_imprimir;

    como_imprimir[typeid(int)] = [](const std::any& a) {
        return fmt::format("int({})", std::any_cast<int>(a));
    };
    como_imprimir[typeid(double)] = [](const std::any& a) {
        return fmt::format("double({:.2f})", std::any_cast<double>(a));
    };
    como_imprimir[typeid(std::string)] = [](const std::any& a) {
        return fmt::format("string(\"{}\")", std::any_cast<std::string>(a));
    };

    std::vector<std::any> vs{7, 2.5, std::string{"oi"}, 'x'};

    fmt::print("      despacho por tipo em runtime, com registro aberto:\n");
    for (const auto& a : vs) {
        auto it = como_imprimir.find(a.type());
        fmt::print("        {}\n",
                   it != como_imprimir.end() ? it->second(a)
                                             : fmt::format("<sem handler para {}>",
                                                           a.type().name()));
    }

    fmt::print("\n      Isto é despacho dinâmico ABERTO: qualquer módulo pode\n");
    fmt::print("      registrar um handler novo sem tocar em nada. É a base do\n");
    fmt::print("      `plugin_registry` (src/plugin_registry/) e do Abstract\n");
    fmt::print("      Factory por string.\n\n");

    fmt::print("      Custo: uma busca em `std::map` (O(log n) + cache miss) por\n");
    fmt::print("      chamada, contra uma indireção só na vtable. Use quando a\n");
    fmt::print("      abertura vale mais que a velocidade.\n\n");

    fmt::print("      ⚠  `typeid(T).name()` é MANGLADO e específico do compilador.\n");
    fmt::print("         Para exibir, use `abi::__cxa_demangle` (GCC/Clang) ou\n");
    fmt::print("         `std::source_location`/`__PRETTY_FUNCTION__` em compile-time.\n");
    fmt::print("         Não serialize esse nome: ele muda entre compiladores.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_std_any();
    demo_shared_deleter();
    demo_move_only();
    demo_type_index();
}

}  // namespace demo_any
