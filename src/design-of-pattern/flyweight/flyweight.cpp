#include "documento.hpp"
#include "fabrica.hpp"
#include "glifo.hpp"

#include <fmt/core.h>

namespace flyweight {

int Glifo::construcoes = 0;

Glifo::Glifo(char c, std::string fonte, int tamanho)
    : caractere_(c), fonte_(std::move(fonte)), tamanho_(tamanho) {
    ++construcoes;
    // Simula a rasterização: um bitmap proporcional ao tamanho da fonte
    bitmap_.assign(static_cast<std::size_t>(tamanho_) * tamanho_, '#');
}

std::string Glifo::desenhar(int x, int y, const std::string& cor) const {
    return fmt::format("'{}'@({},{}) {} {}pt {}", caractere_, x, y,
                       fonte_, tamanho_, cor);
}

std::size_t Glifo::bytes() const {
    return sizeof(Glifo) + fonte_.capacity() + bitmap_.capacity();
}

// ── Fábrica ──────────────────────────────────────────────────────────────────
const Glifo& FabricaDeGlifos::obter(char c, const std::string& fonte, int tamanho) {
    const Chave chave{c, fonte, tamanho};

    if (const auto it = indice_.find(chave); it != indice_.end()) {
        return glifos_[it->second];          // ← reuso: nenhuma construção nova
    }

    glifos_.emplace_back(c, fonte, tamanho);
    indice_.emplace(chave, glifos_.size() - 1);
    return glifos_.back();
}

std::size_t FabricaDeGlifos::bytes_totais() const {
    std::size_t t = 0;
    for (const auto& g : glifos_) t += g.bytes();
    return t;
}

// ── Documento com flyweight ──────────────────────────────────────────────────
void Documento::escrever(const std::string& texto, const std::string& fonte,
                         int tamanho) {
    conteudo_.reserve(conteudo_.size() + texto.size());
    for (char c : texto) {
        if (c == '\n') { x_ = 0; y_ += tamanho; continue; }
        conteudo_.push_back({&fabrica_.obter(c, fonte, tamanho), x_, y_, 0});
        x_ += tamanho / 2;
    }
}

std::string Documento::renderizar(std::size_t max) const {
    std::string s;
    for (std::size_t i = 0; i < conteudo_.size() && i < max; ++i) {
        const auto& c = conteudo_[i];
        s += c.glifo->desenhar(c.x, c.y, "preto") + "\n      ";
    }
    return s;
}

// ── Documento sem flyweight ──────────────────────────────────────────────────
void DocumentoGordo::escrever(const std::string& texto, const std::string& fonte,
                              int tamanho) {
    conteudo_.reserve(conteudo_.size() + texto.size());
    for (char c : texto) {
        if (c == '\n') { x_ = 0; y_ += tamanho; continue; }
        CaractereGordo g;
        g.caractere = c;
        g.fonte     = fonte;                                   // ← cópia
        g.tamanho   = tamanho;
        g.bitmap.assign(static_cast<std::size_t>(tamanho) * tamanho, '#');  // ← cópia
        g.x = x_; g.y = y_; g.cor = 0;
        conteudo_.push_back(std::move(g));
        x_ += tamanho / 2;
    }
}

std::size_t DocumentoGordo::bytes_totais() const {
    std::size_t t = conteudo_.size() * sizeof(CaractereGordo);
    for (const auto& c : conteudo_) t += c.fonte.capacity() + c.bitmap.capacity();
    return t;
}

}  // namespace flyweight
