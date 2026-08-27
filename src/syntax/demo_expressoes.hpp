// =============================================================================
//  src/syntax/demo_expressoes.hpp
//
//  Expressões — operadores, conversões, casts e os operadores de tipo
//  ─────────────────────────────────────────────────────────────────
//  Uma EXPRESSÃO produz um valor; uma INSTRUÇÃO faz algo acontecer. `a + b` é
//  expressão; `a + b;` é instrução. Em C++ quase tudo é expressão — inclusive
//  a atribuição, o que explica o clássico `if (x = 5)` compilar.
//
//  Toda expressão tem duas propriedades independentes:
//      TIPO            → int, double, std::string...
//      CATEGORIA DE VALOR → lvalue (tem endereço) / rvalue (é temporário)
//
//  Tópicos:
//  • Os operadores, por categoria
//  • Pré vs pós-incremento — e por que `++i` é o hábito certo
//  • Precedência e associatividade
//  • Conversões implícitas: promoção, aritmética usual, e a armadilha do unsigned
//  • Os quatro casts nomeados
//  • `sizeof`, `alignof`, `decltype`, `typeid`, o operador `noexcept`
//  • `new` / `delete` — a sintaxe crua sob os smart pointers
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstdint>
#include <new>
#include <string>
#include <typeinfo>
#include <utility>

namespace demo_expressoes {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Os operadores, por categoria
// ─────────────────────────────────────────────────────────────────────────────
void demo_operadores() {
    fmt::print("\n── 7.1 Os operadores ──\n");

    int a = 17, b = 5;

    fmt::print("      Aritméticos (a={}, b={}):\n", a, b);
    fmt::print("        a + b = {}   a - b = {}   a * b = {}\n", a + b, a - b, a * b);
    fmt::print("        a / b = {}    ← divisão INTEIRA trunca (não arredonda)\n", a / b);
    fmt::print("        a % b = {}    ← resto; indefinido se b == 0\n", a % b);
    fmt::print("        a / 5.0 = {:.1f}  ← basta UM operando double para virar real\n", a / 5.0);
    fmt::print("        -a = {}      ← unário\n", -a);

    fmt::print("\n      Relacionais → devolvem bool:\n");
    fmt::print("        a == b {}   a != b {}   a < b {}   a >= b {}\n",
               a == b, a != b, a < b, a >= b);
    fmt::print("        (a <=> b) (C++20) devolve a ORDEM, não um bool — ver src/operators\n");

    fmt::print("\n      Lógicos (com curto-circuito):\n");
    fmt::print("        (a>0) && (b>0) = {}   (a>0) || (b>0) = {}   !(a>0) = {}\n",
               (a > 0) && (b > 0), (a > 0) || (b > 0), !(a > 0));

    fmt::print("\n      Bit a bit (a=0b{:05b}, b=0b{:05b}):\n", a, b);
    fmt::print("        a & b  = 0b{:05b} = {}   ← AND\n",  a & b,  a & b);
    fmt::print("        a | b  = 0b{:05b} = {}   ← OR\n",   a | b,  a | b);
    fmt::print("        a ^ b  = 0b{:05b} = {}   ← XOR\n",  a ^ b,  a ^ b);
    fmt::print("        a << 1 = 0b{:05b} = {}  ← desloca (× 2)\n", a << 1, a << 1);
    fmt::print("        a >> 1 = 0b{:05b} = {}   ← desloca (÷ 2)\n", a >> 1, a >> 1);
    fmt::print("        ~a (em 8 bits) = {}\n", static_cast<int>(static_cast<std::uint8_t>(~a)));
    fmt::print("        ⚠ `&` e `|` NÃO fazem curto-circuito, `&&` e `||` fazem\n");

    fmt::print("\n      Atribuição composta — `x op= y` é `x = x op y` com x avaliado 1 vez:\n");
    int c = 10;
    c += 5; fmt::print("        c += 5  → {}\n", c);
    c -= 3; fmt::print("        c -= 3  → {}\n", c);
    c *= 2; fmt::print("        c *= 2  → {}\n", c);
    c /= 4; fmt::print("        c /= 4  → {}\n", c);
    c %= 4; fmt::print("        c %= 4  → {}\n", c);
    c <<= 3; fmt::print("        c <<= 3 → {}\n", c);
    c |= 1; fmt::print("        c |= 1  → {}\n", c);

    fmt::print("\n      Incremento — a diferença que importa:\n");
    int i = 5, j = 5;
    fmt::print("        int i=5;  valor de (++i) = {}, i = {}  ← incrementa e DEVOLVE o novo\n",
               ++i, i);
    fmt::print("        int j=5;  valor de (j++) = {}, j = {}  ← devolve o ANTIGO, e incrementa\n",
               j++, j);
    fmt::print("        para iteradores, `it++` precisa COPIAR o antigo para devolvê-lo:\n");
    fmt::print("        por isso o hábito em C++ é `++it`, mesmo quando dá no mesmo.\n");

    fmt::print("\n      Os outros:\n");
    fmt::print("        ::  resolução de escopo      .  e  ->  acesso a membro\n");
    fmt::print("        []  índice                   ()  chamada\n");
    fmt::print("        &   endereço-de              *   desreferência\n");
    fmt::print("        ?:  condicional              ,   vírgula\n");
    fmt::print("        .*  e  ->*   ponteiro para membro (ver src/lambdas)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Precedência e associatividade
// ─────────────────────────────────────────────────────────────────────────────
void demo_precedencia() {
    fmt::print("\n── 7.2 Precedência e associatividade ──\n");

    fmt::print("      2 + 3 * 4       = {}   ← `*` antes de `+`\n", 2 + 3 * 4);
    fmt::print("      (2 + 3) * 4     = {}   ← parênteses mandam\n", (2 + 3) * 4);
    fmt::print("      1 << 2 + 3      = {}  ← `+` tem precedência MAIOR que `<<`!\n",
               1 << (2 + 3));
    fmt::print("        vira 1 << 5 = 32, e não (1<<2)+3 = 7\n");
    fmt::print("        (o GCC até avisa: -Wparentheses \"suggest parentheses\")\n");

    int a = 1, b = 2, c = 3;
    a = b = c;                                   // `=` associa à DIREITA
    fmt::print("      a = b = c;  → a={} b={} c={}  ← `=` associa à direita\n", a, b, c);

    fmt::print("\n      Da maior para a menor precedência (resumo):\n");
    fmt::print("        1.  ::                                  esquerda→direita\n");
    fmt::print("        2.  a++  a--  f()  a[]  .  ->           esquerda→direita\n");
    fmt::print("        3.  ++a  --a  !  ~  -a  *p  &a  casts   DIREITA→esquerda\n");
    fmt::print("        4.  .*   ->*\n");
    fmt::print("        5.  *  /  %\n");
    fmt::print("        6.  +  -\n");
    fmt::print("        7.  <<  >>\n");
    fmt::print("        8.  <=>                                 (C++20)\n");
    fmt::print("        9.  <  <=  >  >=\n");
    fmt::print("       10.  ==  !=\n");
    fmt::print("       11.  &        12. ^        13. |\n");
    fmt::print("       14.  &&       15. ||\n");
    fmt::print("       16.  ?:  =  +=  -=  ...                  DIREITA→esquerda\n");
    fmt::print("       17.  ,                                   esquerda→direita\n");

    fmt::print("\n      A regra prática: ninguém decora esta tabela. Ao misturar\n");
    fmt::print("      aritmética com bits, deslocamento ou comparação, ponha parênteses —\n");
    fmt::print("      eles não custam nada e removem a dúvida do leitor.\n");

    fmt::print("\n      ⚠ Precedência ≠ ORDEM DE AVALIAÇÃO. Em `f() + g()`, a precedência\n");
    fmt::print("        não diz quem roda primeiro — e até o C++17 `i = i++ + 1` era UB.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Categorias de valor
// ─────────────────────────────────────────────────────────────────────────────
int  devolve_valor()      { return 42; }
int& devolve_referencia() { static int x = 42; return x; }

void demo_categorias_valor() {
    fmt::print("\n── 7.3 Categorias de valor ──\n");

    int  x = 1;
    int* p = &x;

    fmt::print("      lvalue → tem NOME/ENDEREÇO, sobrevive à expressão:\n");
    fmt::print("        x, *p, arr[0], devolve_referencia()   → &x é legal ({}...)\n",
               static_cast<bool>(p != nullptr));
    fmt::print("      rvalue → temporário, morre no fim da expressão:\n");
    fmt::print("        42, x + 1, devolve_valor(), std::move(x)\n");
    fmt::print("        `&42` e `&(x+1)` não compilam: não há onde apontar\n");

    devolve_referencia() = 99;                   // um lvalue pode ir à ESQUERDA do `=`
    fmt::print("\n      devolve_referencia() = 99;  → agora vale {}\n", devolve_referencia());
    fmt::print("      devolve_valor() = 99;       → ERRO: rvalue não é atribuível\n");

    fmt::print("\n      Isto é a base do move semantics: `T&&` liga-se a rvalues,\n");
    fmt::print("      e um temporário pode ter as entranhas ROUBADAS sem prejuízo.\n");
    fmt::print("      As cinco categorias (lvalue/prvalue/xvalue/glvalue/rvalue) e o\n");
    fmt::print("      perfect forwarding → src/value_semantics.\n");
    fmt::print("        valor de devolve_valor(): {}\n", devolve_valor());
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Conversões implícitas
// ─────────────────────────────────────────────────────────────────────────────
void demo_conversoes() {
    fmt::print("\n── 7.4 Conversões implícitas ──\n");

    char  c = 'A';
    short s = 300;
    fmt::print("      Promoção integral — tudo menor que int vira int nas operações:\n");
    fmt::print("        'A' + 1 → {} (int), não char\n", c + 1);
    fmt::print("        short + short → int: {}\n", s + s);

    fmt::print("\n      Conversões aritméticas usuais — o \"maior\" tipo vence:\n");
    fmt::print("        int + double → double: {}\n", 1 + 2.5);
    fmt::print("        float + double → double\n");
    fmt::print("        int + unsigned → UNSIGNED  ← a armadilha abaixo\n");

    int      assinado    = -1;
    unsigned nao_assinado = 1u;
    fmt::print("\n      ⚠ A armadilha do signed/unsigned:\n");
    fmt::print("        int a = -1;  unsigned b = 1;\n");
    fmt::print("        `a < b` converte a para unsigned: {} vira {}\n",
               assinado, static_cast<unsigned>(assinado));
    fmt::print("        → a comparação dá FALSE, ao contrário de toda intuição\n");
    fmt::print("        (por isso `-Wsign-compare` existe, e por isso comparar\n");
    fmt::print("         `i < v.size()` com `int i` acende um warning)\n");
    fmt::print("        correto: comparar como int → {}\n",
               assinado < static_cast<int>(nao_assinado));

    fmt::print("\n      Truncamento e perda de precisão são SILENCIOSOS:\n");
    double d = 3.99;
    int    t = static_cast<int>(d);
    fmt::print("        double 3.99 → int: {}   ← trunca para zero, não arredonda\n", t);
    fmt::print("        int grande → float: perde precisão acima de 2^24\n");
    fmt::print("        qualquer numérico → bool: 0 é false, o resto é true ({})\n",
               static_cast<bool>(-5));
    fmt::print("        → inicializar com {{ }} BLOQUEIA as conversões que perdem dados\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Os quatro casts nomeados
// ─────────────────────────────────────────────────────────────────────────────
struct Base    { virtual ~Base() = default; };
struct Derivada : Base { int marca = 7; };

void demo_casts() {
    fmt::print("\n── 7.5 Os quatro casts nomeados ──\n");

    double     d = 3.7;
    int        i = static_cast<int>(d);
    const int  ci = 10;
    int&       ref_mutavel = const_cast<int&>(ci);
    Derivada   der;
    Base*      base_ptr = &der;
    auto*      de_volta = dynamic_cast<Derivada*>(base_ptr);
    auto       endereco = reinterpret_cast<std::uintptr_t>(base_ptr);

    fmt::print("      static_cast<int>(3.7)              = {}\n", i);
    fmt::print("        conversões \"razoáveis\", verificadas em compile-time\n");
    fmt::print("      dynamic_cast<Derivada*>(base_ptr)  = {}\n",
               de_volta != nullptr ? "ok (marca " + std::to_string(de_volta->marca) + ")"
                                   : "nullptr");
    fmt::print("        desce na hierarquia COM verificação em runtime (exige virtual)\n");
    fmt::print("        falha → nullptr (ponteiro) ou std::bad_cast (referência)\n");
    fmt::print("      const_cast<int&>(ci)               = {}\n", ref_mutavel);
    fmt::print("        remove const; ESCREVER num objeto originalmente const é UB\n");
    fmt::print("      reinterpret_cast<uintptr_t>(ptr)   = 0x{:x}\n", endereco);
    fmt::print("        reinterpreta os bits; a menos portável e a mais perigosa\n");

    // A ordem completa é a de [expr.cast]/4 — a mesma listada em
    // src/casts/demo_c_style.hpp. Repare que dynamic_cast NUNCA entra na lista:
    // o cast estilo C não faz verificação em runtime.
    fmt::print("\n      E o cast à moda do C, `(int)x`?\n");
    fmt::print("        tenta, nesta ordem (o primeiro que \"funciona\" vence):\n");
    fmt::print("          1. const_cast\n");
    fmt::print("          2. static_cast\n");
    fmt::print("          3. static_cast + const_cast\n");
    fmt::print("          4. reinterpret_cast\n");
    fmt::print("          5. reinterpret_cast + const_cast\n");
    fmt::print("        dynamic_cast NUNCA é tentado — não há checagem em runtime\n");
    fmt::print("        → você não sabe qual foi aplicado, e não dá para procurar no grep\n");
    fmt::print("      Cobertura completa (incluindo bit_cast) → src/casts.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Operadores que consultam tipos
// ─────────────────────────────────────────────────────────────────────────────
struct Alinhado { char c; double d; };

template <typename... Ts>
constexpr std::size_t quantos(Ts&&...) { return sizeof...(Ts); }

void demo_operadores_de_tipo() {
    fmt::print("\n── 7.6 Operadores que consultam tipos ──\n");

    Alinhado a{};

    fmt::print("      sizeof(x)   → tamanho em bytes, em COMPILE-TIME\n");
    fmt::print("        sizeof(int)={}  sizeof(Alinhado)={} (char 1 + padding 7 + double 8)\n",
               sizeof(int), sizeof(Alinhado));
    fmt::print("      alignof(T)  → alinhamento exigido\n");
    fmt::print("        alignof(char)={}  alignof(double)={}  alignof(Alinhado)={}\n",
               alignof(char), alignof(double), alignof(Alinhado));
    fmt::print("      sizeof...(pack) → tamanho de um parameter pack: {}\n",
               quantos(1, 2.0, 'c'));
    fmt::print("      decltype(expr)  → o tipo da expressão, sem avaliá-la\n");
    fmt::print("        decltype(1 + 2.0) é double\n");
    fmt::print("      typeid(x).name() → identificação em runtime (nome MANGLED no GCC)\n");
    fmt::print("        typeid(a).name() = \"{}\"\n", typeid(a).name());
    fmt::print("      noexcept(expr)  → esta expressão promete não lançar? {}\n",
               noexcept(sizeof(int)));

    fmt::print("\n      ⚠ `sizeof` NÃO avalia o operando: `sizeof(f())` não chama `f`.\n");
    fmt::print("      ⚠ `sizeof(array)` só funciona antes do decay para ponteiro.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. new e delete — a sintaxe crua
// ─────────────────────────────────────────────────────────────────────────────
void demo_new_delete() {
    fmt::print("\n── 7.7 new e delete ──\n");

    int* um = new int(42);              // aloca e constrói
    fmt::print("      int* p = new int(42);   → *p = {}\n", *um);
    delete um;                          // destrói e libera
    fmt::print("      delete p;\n");

    int* muitos = new int[5]{1, 2, 3, 4, 5};
    fmt::print("      int* a = new int[5]{{...}};  → a[3] = {}\n", muitos[3]);
    delete[] muitos;                    // ⚠ `delete[]`, não `delete`
    fmt::print("      delete[] a;   ← o par TEM de bater: new[] ↔ delete[]\n");

    int* talvez = new (std::nothrow) int(1);
    fmt::print("      new (std::nothrow) T   → devolve nullptr em vez de lançar: {}\n",
               talvez != nullptr);
    delete talvez;

    fmt::print("\n      As formas de `new`:\n");
    fmt::print("        new T          | new T(args)  | new T{{args}}\n");
    fmt::print("        new T[n]       | new (ptr) T  ← placement new: constrói NO endereço\n");

    fmt::print("\n      ⚠ Cada `new` cru é um vazamento esperando por uma exceção:\n");
    fmt::print("          T* p = new T; f(); delete p;   ← se f() lançar, `delete` nunca roda\n");
    fmt::print("      Em C++ moderno a sintaxe que se usa é outra:\n");
    fmt::print("          auto p = std::make_unique<T>();   ← libera sozinho, sempre\n");
    fmt::print("          std::vector<T> v(n);              ← em vez de new T[n]\n");
    fmt::print("      Ver src/smart-pointers e src/memory (pool, arena, pmr).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_operadores();
    demo_precedencia();
    demo_categorias_valor();
    demo_conversoes();
    demo_casts();
    demo_operadores_de_tipo();
    demo_new_delete();
}

} // namespace demo_expressoes
