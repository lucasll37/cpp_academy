#pragma once
// =============================================================================
//  proxies.hpp — as três variantes
// =============================================================================
#include "imagem.hpp"

#include <memory>
#include <string>
#include <vector>

namespace proxy {

// ─────────────────────────────────────────────────────────────────────────────
//  1. PROXY VIRTUAL — adia a construção cara até o primeiro uso
//
//  O uso mais comum do padrão. Note o `mutable`: `desenhar()` é `const` do
//  ponto de vista lógico (não muda a imagem), mas precisa materializar o
//  objeto real. É exatamente o caso de uso de `mutable`.
// ─────────────────────────────────────────────────────────────────────────────
class ImagemPreguicosa final : public Imagem {
public:
    explicit ImagemPreguicosa(std::string arquivo)
        : arquivo_(std::move(arquivo)) {}

    void        desenhar() const override;
    std::size_t bytes() const override;
    std::string nome() const override { return arquivo_; }

    bool ja_carregou() const { return real_ != nullptr; }

private:
    void garantir_carregada() const;

    std::string                        arquivo_;
    mutable std::unique_ptr<ImagemReal> real_;   // ← mutable: lazy num const
};

// ─────────────────────────────────────────────────────────────────────────────
//  2. PROXY DE PROTEÇÃO — verifica antes de repassar
// ─────────────────────────────────────────────────────────────────────────────
enum class Papel { Anonimo, Usuario, Administrador };

class ImagemProtegida final : public Imagem {
public:
    ImagemProtegida(std::shared_ptr<Imagem> real, Papel minimo)
        : real_(std::move(real)), minimo_(minimo) {}

    void definir_papel_atual(Papel p) { atual_ = p; }

    void        desenhar() const override;
    std::size_t bytes() const override;
    std::string nome() const override { return real_->nome(); }

private:
    bool permitido() const { return atual_ >= minimo_; }

    std::shared_ptr<Imagem> real_;
    Papel                   minimo_;
    Papel                   atual_ = Papel::Anonimo;
};

// ─────────────────────────────────────────────────────────────────────────────
//  3. SMART PROXY — instrumenta o acesso
// ─────────────────────────────────────────────────────────────────────────────
class ImagemInstrumentada final : public Imagem {
public:
    explicit ImagemInstrumentada(std::shared_ptr<Imagem> real)
        : real_(std::move(real)) {}

    void        desenhar() const override;
    std::size_t bytes() const override;
    std::string nome() const override { return real_->nome(); }

    int acessos() const { return acessos_; }

private:
    std::shared_ptr<Imagem> real_;
    mutable int             acessos_ = 0;
};

}  // namespace proxy
