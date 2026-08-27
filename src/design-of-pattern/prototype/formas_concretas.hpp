#pragma once
// =============================================================================
//  formas_concretas.hpp — as implementações
//
//  Cada uma implementa `clonar()` da mesma forma:
//      return std::make_unique<EuMesmo>(*this);
//
//  Isso é boilerplate, e a seção 4 do main.cpp mostra como eliminá-lo com CRTP.
// =============================================================================
#include "forma.hpp"

#include <memory>
#include <string>
#include <vector>

namespace prototype {

class Circulo : public Forma {
public:
    explicit Circulo(double raio) : raio_(raio) {}

    std::unique_ptr<Forma> clonar() const override {
        // O construtor de CÓPIA gerado copia raio_ E cor_ (herdado).
        // É a Rule of 0 fazendo o trabalho — ver src/value_semantics/.
        return std::make_unique<Circulo>(*this);
    }

    std::string descrever() const override;
    double      area() const override;

    void definir_raio(double r) { raio_ = r; }

private:
    double raio_;
};

class Retangulo : public Forma {
public:
    Retangulo(double l, double a) : largura_(l), altura_(a) {}

    std::unique_ptr<Forma> clonar() const override {
        return std::make_unique<Retangulo>(*this);
    }

    std::string descrever() const override;
    double      area() const override;

private:
    double largura_, altura_;
};

// Uma forma COMPOSTA: o clone precisa ser PROFUNDO, e é aqui que o padrão
// deixa de ser trivial.
class Grupo : public Forma {
public:
    Grupo() = default;

    // Construtor de cópia ESCRITO À MÃO: o gerado copiaria os `unique_ptr`,
    // o que nem compila (unique_ptr não é copiável). Precisamos clonar cada
    // filho — é a diferença entre cópia rasa e profunda.
    Grupo(const Grupo& outro) : Forma(outro) {
        filhos_.reserve(outro.filhos_.size());
        for (const auto& f : outro.filhos_) {
            filhos_.push_back(f->clonar());       // ← clone recursivo
        }
    }
    Grupo& operator=(const Grupo&) = delete;
    Grupo(Grupo&&) = default;
    Grupo& operator=(Grupo&&) = default;

    std::unique_ptr<Forma> clonar() const override {
        return std::make_unique<Grupo>(*this);
    }

    std::string descrever() const override;
    double      area() const override;

    void adicionar(std::unique_ptr<Forma> f) { filhos_.push_back(std::move(f)); }
    std::size_t tamanho() const { return filhos_.size(); }
    const Forma& filho(std::size_t i) const { return *filhos_[i]; }
    Forma&       filho(std::size_t i)       { return *filhos_[i]; }

private:
    std::vector<std::unique_ptr<Forma>> filhos_;
};

}  // namespace prototype
