#pragma once
// =============================================================================
//  forma.hpp — a interface com o método de clonagem
//
//  O PROTOTYPE resolve um problema específico: criar uma cópia de um objeto
//  cujo TIPO CONCRETO você não conhece.
//
//      Forma* f = obter_de_algum_lugar();
//      Forma* copia = new Forma(*f);          // ✗ Forma é abstrata
//      Forma* copia = new ???(*f);            // ✗ que tipo?
//      auto copia = f->clonar();              // ✓ o objeto sabe o que é
//
//  É o mesmo raciocínio do destrutor virtual (src/oop/demo_virtual.hpp, 2.4):
//  a operação depende do tipo DINÂMICO, então precisa ser virtual.
// =============================================================================
#include <memory>
#include <string>

namespace prototype {

class Forma {
public:
    virtual ~Forma() = default;

    // O método-chave. Devolver `unique_ptr` em vez de `Forma*` é a diferença
    // entre o padrão de 1994 e o de hoje: a posse fica explícita e não vaza.
    virtual std::unique_ptr<Forma> clonar() const = 0;

    virtual std::string descrever() const = 0;
    virtual double      area() const = 0;

    // Estado comum, para deixar visível que o clone copia TUDO
    void definir_cor(std::string c) { cor_ = std::move(c); }
    const std::string& cor() const { return cor_; }

protected:
    std::string cor_ = "preto";
};

}  // namespace prototype
