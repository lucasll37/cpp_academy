// =============================================================================
//  src/oop/demo_inheritance.hpp
//
//  Herança — o mecanismo, antes do polimorfismo
//  ────────────────────────────────────────────
//  Herança em C++ faz duas coisas que costumam ser confundidas:
//    1. reutilização de implementação (o que os membros herdados fazem)
//    2. substituibilidade de tipo     (onde um Derivado pode ficar no lugar de Base)
//
//  Só a herança PÚBLICA faz a segunda. As outras duas formas existem, são úteis,
//  e não criam relação "é um".
//
//  Tópicos:
//  • public / protected / private — o que cada uma significa
//  • Ordem de construção e destruição
//  • Herdando construtores com `using`
//  • Ocultação de nome (name hiding) e como desfazer
//  • Composição como alternativa padrão
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <type_traits>
#include <utility>

namespace demo_inheritance {

// ─────────────────────────────────────────────────────────────────────────────
//  1. As três formas de herdar
//
//     A palavra-chave controla o acesso MÁXIMO que os membros herdados terão
//     visto de fora do derivado:
//
//       herança pública    : public → public,    protected → protected
//       herança protegida  : public → protected, protected → protected
//       herança privada    : public → private,   protected → private
//
//     Em todos os casos, membros PRIVADOS da base continuam inacessíveis.
// ─────────────────────────────────────────────────────────────────────────────
struct Motor {
    void ligar()   { fmt::print("      Motor::ligar()\n"); }
    void desligar(){ fmt::print("      Motor::desligar()\n"); }
protected:
    int rotacao_ = 0;
private:
    int numero_serie_ = 12345;      // invisível para qualquer derivado
};

// Herança PÚBLICA — cria a relação "é um". Carro É UM Motor? Não. Modelagem ruim,
// mas serve para mostrar a conversão implícita que só esta forma habilita.
struct MotorEletrico : public Motor {
    void status() { fmt::print("      MotorEletrico: rotação = {}\n", rotacao_); }
};

// Herança PRIVADA — significa "é implementado em termos de". Nenhuma conversão
// implícita para Motor existe fora da classe.
class Carro : private Motor {
public:
    void dar_partida() {
        fmt::print("      Carro::dar_partida() → delegando\n");
        ligar();                    // acessível AQUI DENTRO
    }
    // `using` reexpõe seletivamente um membro herdado privadamente
    using Motor::desligar;
};

void demo_formas_de_heranca() {
    fmt::print("\n── 1.1 public / protected / private ──\n");

    MotorEletrico me;
    me.ligar();                     // público → herdado como público
    me.status();

    Motor& ref = me;                // ✓ conversão implícita: herança pública
    ref.desligar();

    fmt::print("\n");
    Carro c;
    c.dar_partida();
    c.desligar();                   // só funciona por causa do `using Motor::desligar`
    // c.ligar();                   // ✗ erro: herdado como privado
    // Motor& r2 = c;               // ✗ erro: herança privada não converte

    fmt::print("      Conversão implícita para a base:\n");
    fmt::print("        MotorEletrico (pública) : {}\n",
               std::is_convertible_v<MotorEletrico*, Motor*> ? "sim" : "não");
    fmt::print("        Carro         (privada) : {}\n",
               std::is_convertible_v<Carro*, Motor*> ? "sim" : "não");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Ordem de construção e destruição
//
//     Construção: base → membros (na ordem de DECLARAÇÃO) → corpo do ctor
//     Destruição: exatamente o inverso.
//
//     Os membros são inicializados na ordem em que foram DECLARADOS na classe,
//     não na ordem em que aparecem na lista de inicialização.
// ─────────────────────────────────────────────────────────────────────────────
struct Peca {
    std::string nome;
    explicit Peca(std::string n) : nome(std::move(n)) {
        fmt::print("        + Peca('{}')\n", nome);
    }
    ~Peca() { fmt::print("        - Peca('{}')\n", nome); }
};

struct Base {
    Peca peca_base{"peça-da-base"};
    Base()  { fmt::print("      + Base::Base()\n"); }
    ~Base() { fmt::print("      - Base::~Base()\n"); }
};

struct Derivada : Base {
    Peca primeira{"primeira"};      // declarada antes → construída antes
    Peca segunda{"segunda"};

    // A ordem da lista de inicialização é IGNORADA — vale a ordem de declaração
    // acima. Escrevê-la fora de ordem (`: segunda(...), primeira(...)`) compila,
    // roda na ordem de declaração mesmo assim, e rende um -Wreorder.
    Derivada() : primeira("primeira"), segunda("segunda") {
        fmt::print("      + Derivada::Derivada()\n");
    }
    ~Derivada() { fmt::print("      - Derivada::~Derivada()\n"); }
};

void demo_ordem() {
    fmt::print("\n── 1.2 Ordem de construção e destruição ──\n");
    fmt::print("      construindo Derivada:\n");
    {
        Derivada d;
        fmt::print("      (objeto pronto)\n");
        fmt::print("      destruindo:\n");
    }
    fmt::print("      Construção: base → membros (ordem de DECLARAÇÃO) → corpo\n");
    fmt::print("      Destruição: exatamente o inverso\n");
    fmt::print("      ⚠  a ordem da lista de inicialização é ignorada —\n");
    fmt::print("         escrevê-la fora de ordem gera -Wreorder\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Herdando construtores com `using`
//
//     Sem o `using`, cada construtor da base precisaria ser reescrito à mão
//     no derivado — puro boilerplate.
// ─────────────────────────────────────────────────────────────────────────────
struct Conexao {
    std::string host;
    int         porta;

    Conexao(std::string h, int p) : host(std::move(h)), porta(p) {}
    explicit Conexao(std::string h) : host(std::move(h)), porta(80) {}
};

struct ConexaoSegura : Conexao {
    using Conexao::Conexao;         // herda TODOS os construtores acima

    // Membros próprios do derivado precisam de inicializador default,
    // porque os construtores herdados não sabem inicializá-los.
    bool tls = true;
};

void demo_herdar_construtores() {
    fmt::print("\n── 1.3 Herdando construtores ──\n");

    ConexaoSegura a{"api.exemplo.com", 443};
    ConexaoSegura b{"exemplo.com"};

    fmt::print("      a = {}:{} (tls={})\n", a.host, a.porta, a.tls);
    fmt::print("      b = {}:{} (tls={})\n", b.host, b.porta, b.tls);
    fmt::print("      `using Conexao::Conexao;` herdou os 2 construtores\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Ocultação de nome (name hiding)
//
//     Declarar QUALQUER membro com um nome no derivado esconde TODOS os
//     overloads daquele nome na base — mesmo os de assinatura diferente.
//     A busca de nome para quando encontra o nome, antes da resolução de overload.
// ─────────────────────────────────────────────────────────────────────────────
struct Impressora {
    void imprimir(int i)                { fmt::print("      Impressora::imprimir(int) = {}\n", i); }
    void imprimir(const std::string& s) { fmt::print("      Impressora::imprimir(str) = {}\n", s); }
};

struct ImpressoraRuim : Impressora {
    void imprimir(double d) { fmt::print("      ImpressoraRuim::imprimir(double) = {}\n", d); }
    // Este único método escondeu os DOIS overloads da base.
};

struct ImpressoraBoa : Impressora {
    using Impressora::imprimir;         // traz de volta todos os overloads da base
    void imprimir(double d) { fmt::print("      ImpressoraBoa::imprimir(double) = {}\n", d); }
};

void demo_name_hiding() {
    fmt::print("\n── 1.4 Ocultação de nome ──\n");

    ImpressoraRuim ruim;
    ruim.imprimir(3.14);
    ruim.imprimir(42);           // ⚠ NÃO chama imprimir(int): converte 42 → 42.0!
    fmt::print("        ↑ 42 virou double: os overloads da base estão escondidos\n");
    // ruim.imprimir("texto");   // ✗ erro: nem compila

    fmt::print("\n");
    ImpressoraBoa boa;
    boa.imprimir(3.14);
    boa.imprimir(42);            // ✓ agora acha imprimir(int)
    boa.imprimir(std::string("texto"));
    fmt::print("        ↑ `using Impressora::imprimir;` restaurou os overloads\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Composição — o default que deveria ser
//
//     "Prefira composição a herança" não é dogma: composição não acopla o
//     derivado ao layout e ao ciclo de vida da base, e a interface exposta
//     é exatamente a que você escreveu.
// ─────────────────────────────────────────────────────────────────────────────
class CarroComposto {
public:
    void dar_partida() {
        fmt::print("      CarroComposto::dar_partida()\n");
        motor_.ligar();
    }
    void parar() { motor_.desligar(); }

private:
    Motor motor_;                   // TEM UM motor, não É UM motor
};

void demo_composicao() {
    fmt::print("\n── 1.5 Composição como alternativa ──\n");

    CarroComposto c;
    c.dar_partida();
    c.parar();

    fmt::print("      Herança privada e composição resolvem o mesmo problema.\n");
    fmt::print("      Prefira composição, exceto quando você precisa:\n");
    fmt::print("        • sobrescrever um método virtual da base, ou\n");
    fmt::print("        • usar a base vazia sem pagar por ela (EBO).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_formas_de_heranca();
    demo_ordem();
    demo_herdar_construtores();
    demo_name_hiding();
    demo_composicao();
}

} // namespace demo_inheritance
