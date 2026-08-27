#pragma once
// =============================================================================
//  documento.hpp — o CLIENTE, que guarda o estado extrínseco
//
//  Cada caractere na tela é um `CaractereNaTela`: uma REFERÊNCIA ao glifo
//  compartilhado mais a posição e a cor, que são únicas.
//
//  Compare os dois `sizeof` no main.cpp — é aí que a economia aparece.
// =============================================================================
#include "fabrica.hpp"
#include "glifo.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace flyweight {

// O estado EXTRÍNSECO, um por caractere na tela
struct CaractereNaTela {
    const Glifo* glifo;      // ← ponteiro para o compartilhado (8 bytes)
    int          x, y;       // ← único
    std::uint8_t cor;        // ← único (índice numa paleta)
};

class Documento {
public:
    explicit Documento(FabricaDeGlifos& fabrica) : fabrica_(fabrica) {}

    void escrever(const std::string& texto, const std::string& fonte, int tamanho);

    std::string renderizar(std::size_t max_caracteres) const;

    std::size_t caracteres() const { return conteudo_.size(); }

    // A memória que o DOCUMENTO ocupa (sem contar os glifos compartilhados)
    std::size_t bytes_do_documento() const {
        return conteudo_.size() * sizeof(CaractereNaTela);
    }

private:
    FabricaDeGlifos&             fabrica_;
    std::vector<CaractereNaTela> conteudo_;
    int                          x_ = 0, y_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  A versão SEM flyweight, para comparar
//
//  Cada caractere carrega uma cópia COMPLETA do estado da fonte.
// ─────────────────────────────────────────────────────────────────────────────
struct CaractereGordo {
    char         caractere;
    std::string  fonte;      // ← COPIADO em cada caractere
    int          tamanho;
    std::string  bitmap;     // ← COPIADO em cada caractere
    int          x, y;
    std::uint8_t cor;
};

class DocumentoGordo {
public:
    void escrever(const std::string& texto, const std::string& fonte, int tamanho);

    std::size_t caracteres() const { return conteudo_.size(); }
    std::size_t bytes_totais() const;

private:
    std::vector<CaractereGordo> conteudo_;
    int                         x_ = 0, y_ = 0;
};

}  // namespace flyweight
