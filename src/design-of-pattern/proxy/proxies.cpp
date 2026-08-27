#include "imagem.hpp"
#include "proxies.hpp"

#include <fmt/core.h>

namespace proxy {

int ImagemReal::construcoes = 0;

ImagemReal::ImagemReal(std::string arquivo)
    : arquivo_(std::move(arquivo)), bytes_(arquivo_.size() * 100000) {
    ++construcoes;
    fmt::print("      [real] CARREGANDO '{}' do disco ({} B)...\n", arquivo_, bytes_);
}

void ImagemReal::desenhar() const {
    fmt::print("      [real] desenhando '{}'\n", arquivo_);
}

// ── Proxy virtual ────────────────────────────────────────────────────────────
void ImagemPreguicosa::garantir_carregada() const {
    if (!real_) real_ = std::make_unique<ImagemReal>(arquivo_);
}

void ImagemPreguicosa::desenhar() const {
    garantir_carregada();
    real_->desenhar();
}

std::size_t ImagemPreguicosa::bytes() const {
    garantir_carregada();
    return real_->bytes();
}

// ── Proxy de proteção ────────────────────────────────────────────────────────
void ImagemProtegida::desenhar() const {
    if (!permitido()) {
        fmt::print("      [proteção] acesso NEGADO a '{}'\n", real_->nome());
        return;
    }
    real_->desenhar();
}

std::size_t ImagemProtegida::bytes() const {
    return permitido() ? real_->bytes() : 0;
}

// ── Smart proxy ──────────────────────────────────────────────────────────────
void ImagemInstrumentada::desenhar() const {
    ++acessos_;
    fmt::print("      [instr] acesso #{} a '{}'\n", acessos_, real_->nome());
    real_->desenhar();
}

std::size_t ImagemInstrumentada::bytes() const {
    ++acessos_;
    return real_->bytes();
}

}  // namespace proxy
