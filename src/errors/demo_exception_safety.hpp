// =============================================================================
//  src/errors/demo_exception_safety.hpp
//
//  Garantias de exceção e o contrato do `noexcept`
//  ───────────────────────────────────────────────
//  "É seguro contra exceções" não é sim ou não — são QUATRO níveis. Toda função
//  que você escreve oferece um deles, mesmo que você não tenha escolhido qual.
//
//    nenhuma  — pode vazar recurso ou deixar o objeto corrompido
//    básica   — nada vaza, invariantes preservados, estado não especificado
//    forte    — ou funcionou por completo, ou nada mudou (commit-or-rollback)
//    nothrow  — não lança, ponto
//
//  Tópicos:
//  • As 4 garantias, com o mesmo código em 3 versões
//  • copy-and-swap para conseguir a garantia forte
//  • Por que o destrutor NÃO pode lançar
//  • O contrato do `noexcept` e std::terminate
//  • std::vector::push_back como estudo de caso
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo_exception_safety {

// ─────────────────────────────────────────────────────────────────────────────
//  Um tipo que lança sob demanda, para forçar as falhas nos pontos certos
// ─────────────────────────────────────────────────────────────────────────────
struct Recurso {
    std::string nome;
    static inline bool lancar_no_proximo = false;

    explicit Recurso(std::string n) : nome(std::move(n)) {
        if (lancar_no_proximo) {
            lancar_no_proximo = false;
            throw std::runtime_error(fmt::format("falha ao construir '{}'", nome));
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Garantia NENHUMA — o código que vaza
// ─────────────────────────────────────────────────────────────────────────────
class ContaSemGarantia {
public:
    ContaSemGarantia() : historico_(new std::vector<std::string>()) {}
    ~ContaSemGarantia() { delete historico_; }

    ContaSemGarantia(const ContaSemGarantia&)            = delete;
    ContaSemGarantia& operator=(const ContaSemGarantia&) = delete;

    void registrar(const std::string& op) {
        saldo_ += 100;                          // (1) mudou o estado
        historico_->push_back(op);              // (2) pode lançar (bad_alloc)
        contador_ += 1;                         // (3) só roda se (2) passou
        // Se (2) lançar, o saldo já mudou mas o contador não: os dois campos
        // ficam inconsistentes. Invariante quebrada — garantia NENHUMA.
    }

    int saldo() const noexcept    { return saldo_; }
    int contador() const noexcept { return contador_; }

private:
    int saldo_    = 0;
    int contador_ = 0;
    std::vector<std::string>* historico_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  2. Garantia BÁSICA — nada vaza, invariantes valem, valores não especificados
// ─────────────────────────────────────────────────────────────────────────────
class ContaBasica {
public:
    void registrar(const std::string& op) {
        historico_.push_back(op);               // se lançar, nada foi alterado ainda
        saldo_    += 100;                       // estas duas não lançam
        contador_ += 1;
        // Ordenar a operação que PODE lançar primeiro já eleva a garantia.
    }

    int saldo() const noexcept    { return saldo_; }
    int contador() const noexcept { return contador_; }

private:
    int saldo_    = 0;
    int contador_ = 0;
    std::vector<std::string> historico_;        // RAII: nada vaza
};

// ─────────────────────────────────────────────────────────────────────────────
//  3. Garantia FORTE via copy-and-swap
//
//     A receita: faça todo o trabalho arriscado numa CÓPIA. Só quando tudo
//     der certo, troque a cópia com o original — e `swap` é nothrow.
// ─────────────────────────────────────────────────────────────────────────────
class ContaForte {
public:
    void registrar_varios(const std::vector<std::string>& ops) {
        ContaForte copia = *this;               // 1. trabalha numa cópia

        for (const auto& op : ops) {            // 2. tudo que pode lançar
            copia.historico_.push_back(op);     //    acontece na cópia
            copia.saldo_    += 100;
            copia.contador_ += 1;
            if (op == "INVÁLIDA") {
                throw std::invalid_argument("operação inválida: " + op);
            }
        }

        trocar(*this, copia);                   // 3. commit — nothrow
    }

    friend void trocar(ContaForte& a, ContaForte& b) noexcept {
        std::swap(a.saldo_,      b.saldo_);
        std::swap(a.contador_,   b.contador_);
        std::swap(a.historico_,  b.historico_);
    }

    int saldo() const noexcept    { return saldo_; }
    int contador() const noexcept { return contador_; }

private:
    int saldo_    = 0;
    int contador_ = 0;
    std::vector<std::string> historico_;
};

void demo_garantias() {
    fmt::print("\n── 2.1 As quatro garantias ──\n");

    // BÁSICA
    ContaBasica basica;
    basica.registrar("depósito");
    fmt::print("      básica após 1 operação: saldo={} contador={}\n",
               basica.saldo(), basica.contador());

    // FORTE — a operação falha no meio, mas o objeto não muda NADA
    ContaForte forte;
    forte.registrar_varios({"a", "b"});
    fmt::print("      forte após 2 operações: saldo={} contador={}\n",
               forte.saldo(), forte.contador());

    try {
        forte.registrar_varios({"c", "d", "INVÁLIDA", "e"});
    } catch (const std::invalid_argument& e) {
        fmt::print("      falhou no meio: {}\n", e.what());
    }
    fmt::print("      forte APÓS a falha:     saldo={} contador={}\n",
               forte.saldo(), forte.contador());
    fmt::print("      ↑ inalterado: 'c' e 'd' também foram desfeitos.\n");
    fmt::print("        Ou tudo, ou nada — é a garantia forte.\n");

    fmt::print("\n      | garantia | promessa                                  |\n");
    fmt::print("      |----------|-------------------------------------------|\n");
    fmt::print("      | nenhuma  | nada — pode vazar ou corromper            |\n");
    fmt::print("      | básica   | não vaza, invariantes valem, valores ???  |\n");
    fmt::print("      | forte    | ou completou, ou nada mudou               |\n");
    fmt::print("      | nothrow  | não lança                                 |\n");
    fmt::print("      Mire na BÁSICA sempre; na FORTE quando o custo permitir.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. A ordem das operações já muda a garantia
//
//     Regra prática que resolve metade dos casos:
//     faça primeiro o que pode lançar, depois o que não pode.
// ─────────────────────────────────────────────────────────────────────────────
class Cache {
public:
    // ✗ ordem ruim: se `new` lançar, o contador já foi incrementado
    void inserir_ruim(const std::string& chave) {
        ++insercoes_;
        itens_.push_back(std::make_unique<std::string>(chave));
    }

    // ✓ ordem boa: o passo arriscado primeiro; o resto é nothrow
    void inserir_bom(const std::string& chave) {
        auto novo = std::make_unique<std::string>(chave);   // pode lançar
        itens_.push_back(std::move(novo));                  // pode lançar (realoc)
        ++insercoes_;                                       // nothrow, por último
    }

    std::size_t tamanho() const noexcept { return itens_.size(); }
    int insercoes() const noexcept       { return insercoes_; }

private:
    std::vector<std::unique_ptr<std::string>> itens_;
    int insercoes_ = 0;
};

void demo_ordem_das_operacoes() {
    fmt::print("\n── 2.2 A ordem das operações define a garantia ──\n");

    Cache c;
    c.inserir_bom("chave-1");
    c.inserir_bom("chave-2");

    fmt::print("      cache: {} itens, {} inserções contadas — consistente\n",
               c.tamanho(), c.insercoes());
    fmt::print("      Regra: o que PODE lançar vem primeiro; as atualizações\n");
    fmt::print("      de estado que NÃO lançam vêm por último.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Destrutores não podem lançar
//
//     Se uma exceção já está em voo e um destrutor lança durante o unwinding,
//     há DUAS exceções ativas — e o padrão não define uma resposta: chama
//     std::terminate. Por isso todo destrutor é implicitamente `noexcept`.
// ─────────────────────────────────────────────────────────────────────────────
struct ArquivoSeguro {
    std::string nome;
    explicit ArquivoSeguro(std::string n) : nome(std::move(n)) {}

    // Destrutor implicitamente noexcept: se algo puder falhar aqui,
    // ele PRECISA engolir o erro.
    ~ArquivoSeguro() {
        try {
            fechar();                        // operação que poderia lançar
        } catch (const std::exception& e) {
            // Logar e engolir. Nunca deixar escapar de um destrutor.
            fmt::print("        [dtor] erro engolido ao fechar '{}': {}\n",
                       nome, e.what());
        }
    }

    // Se o usuário QUER saber da falha, ofereça um método explícito:
    // ele chama `fechar()` antes de o objeto morrer e trata o erro.
    void fechar() {
        if (nome == "corrompido") {
            throw std::runtime_error("falha ao gravar o buffer final");
        }
        fmt::print("        [dtor] '{}' fechado normalmente\n", nome);
    }
};

void demo_destrutor_nao_lanca() {
    fmt::print("\n── 2.3 Destrutores não podem lançar ──\n");

    { ArquivoSeguro ok("relatorio.txt"); }
    { ArquivoSeguro ruim("corrompido"); }

    fmt::print("      Todo destrutor é implicitamente `noexcept`.\n");
    fmt::print("      Se ele lançasse durante o unwinding de OUTRA exceção,\n");
    fmt::print("      haveria duas exceções em voo → std::terminate.\n");
    fmt::print("      Padrão: engolir no destrutor + oferecer um `fechar()`\n");
    fmt::print("      público para quem quiser tratar o erro de verdade.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. O contrato do `noexcept`
//
//     `noexcept` NÃO impede a função de lançar — ele PROMETE que ela não lança.
//     Se a promessa for quebrada, não há unwinding nem handler: o runtime
//     chama std::terminate direto.
//
//     Em troca da promessa, o compilador e a biblioteca otimizam:
//     `std::vector` só usa o move ctor ao realocar se ele for `noexcept`
//     (ver src/value_semantics/demo_rule_of_five.hpp).
// ─────────────────────────────────────────────────────────────────────────────
void nunca_lanca() noexcept { }

void talvez_lance(bool b) { if (b) throw std::runtime_error("lancei"); }

// A promessa pode ser CONDICIONAL: "sou noexcept se a operação interna for".
// É assim que a biblioteca padrão propaga a informação por tipos genéricos.
template <typename T>
void mover_condicional(T& destino, T& origem)
    noexcept(std::is_nothrow_move_assignable_v<T>) {
    destino = std::move(origem);
}

void demo_noexcept() {
    fmt::print("\n── 2.4 O contrato do `noexcept` ──\n");

    // `noexcept(expr)` é um OPERADOR: pergunta em compilação se `expr` pode lançar
    fmt::print("      noexcept(nunca_lanca())        = {}\n", noexcept(nunca_lanca()));
    fmt::print("      noexcept(talvez_lance(true))   = {}\n", noexcept(talvez_lance(true)));

    std::string a = "destino", b = "origem";
    fmt::print("      noexcept(mover_condicional(a,b)) para std::string = {}\n",
               noexcept(mover_condicional(a, b)));

    fmt::print("\n      Uma função `noexcept` que lança NÃO desenrola a pilha:\n");
    fmt::print("        void f() noexcept {{ throw std::runtime_error(\"x\"); }}\n");
    fmt::print("        → std::terminate() imediato, nem um catch(...) salva\n");
    fmt::print("      A linha está fora do demo de propósito: ela ABORTA o programa.\n");

    fmt::print("\n      Marque `noexcept`:\n");
    fmt::print("        • em construtores e atribuições de MOVE (obrigatório na prática)\n");
    fmt::print("        • em `swap`\n");
    fmt::print("        • em destrutores (já é implícito)\n");
    fmt::print("        • em getters e operações triviais\n");
    fmt::print("      Não marque quando a função pode legitimamente falhar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Estudo de caso: std::vector::push_back
//
//     push_back oferece a garantia FORTE. Ao realocar, ele precisa transferir
//     os elementos existentes para o buffer novo — e essa transferência não
//     pode falhar no meio, ou os dois buffers ficariam parciais.
//
//     Solução: `std::move_if_noexcept`.
//       • move ctor noexcept → MOVE (rápido, e não pode falhar)
//       • senão              → COPIA (lento, mas o original fica intacto)
// ─────────────────────────────────────────────────────────────────────────────
void demo_vector_push_back() {
    fmt::print("\n── 2.5 Estudo de caso: std::vector::push_back ──\n");

    std::vector<std::string> v;
    v.reserve(2);
    v.push_back("a");
    v.push_back("b");

    fmt::print("      antes da realocação: size={} capacity={}\n", v.size(), v.capacity());
    v.push_back("c");                       // força realocação
    fmt::print("      após  a realocação: size={} capacity={}\n", v.size(), v.capacity());

    fmt::print("      std::string tem move noexcept? {}\n",
               std::is_nothrow_move_constructible_v<std::string>);
    fmt::print("      → o vector MOVEU os elementos. Se não fosse noexcept,\n");
    fmt::print("        ele teria COPIADO, para poder desfazer em caso de falha.\n");
    fmt::print("      A garantia forte do push_back é paga pelo `noexcept` do\n");
    fmt::print("      seu tipo. Sem ele, você paga em performance.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_garantias();
    demo_ordem_das_operacoes();
    demo_destrutor_nao_lanca();
    demo_noexcept();
    demo_vector_push_back();
}

} // namespace demo_exception_safety
