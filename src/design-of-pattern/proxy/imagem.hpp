#pragma once
// =============================================================================
//  imagem.hpp — o SUJEITO e os quatro tipos de Proxy
//
//  O Proxy mantém a MESMA interface do objeto real e controla o acesso a ele.
//  A diferença para o Decorator (design-of-pattern/decorator/) é a intenção:
//
//    Decorator → ACRESCENTA comportamento; empilha-se
//    Proxy     → CONTROLA o acesso; não se empilha (normalmente)
//
//  O GoF cataloga quatro variantes, e este demo implementa três:
//
//    virtual   — adia a criação cara até o primeiro uso (lazy)
//    proteção  — verifica permissão antes de repassar
//    remoto    — o objeto real está noutro processo/máquina
//    smart     — acrescenta contagem de referência, log, cache
//
//  ⚠ `std::shared_ptr` e `std::unique_ptr` SÃO smart proxies — este é o padrão
//    mais usado do catálogo, e quase ninguém o chama pelo nome.
// =============================================================================
#include <cstddef>
#include <string>

namespace proxy {

class Imagem {
public:
    virtual ~Imagem() = default;
    virtual void        desenhar() const = 0;
    virtual std::size_t bytes() const = 0;
    virtual std::string nome() const = 0;
};

// ── O objeto REAL: caro de construir ─────────────────────────────────────────
class ImagemReal final : public Imagem {
public:
    explicit ImagemReal(std::string arquivo);

    void        desenhar() const override;
    std::size_t bytes() const override { return bytes_; }
    std::string nome() const override  { return arquivo_; }

    static int construcoes;   // para o demo contar quantas de fato aconteceram

private:
    std::string arquivo_;
    std::size_t bytes_;
};

}  // namespace proxy
