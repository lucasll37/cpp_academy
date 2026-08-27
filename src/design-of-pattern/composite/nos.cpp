#include "arquivo.hpp"
#include "pasta.hpp"

#include <fmt/core.h>

#include <algorithm>

namespace composite {

void Arquivo::imprimir_em(int nivel) const {
    fmt::print("    {:>{}}{} ({} B)\n", "", nivel * 2, nome_, bytes_);
}

std::size_t Pasta::tamanho() const {
    std::size_t total = 0;
    for (const auto& f : filhos_) total += f->tamanho();   // ← recursão
    return total;
}

std::size_t Pasta::contar_arquivos() const {
    std::size_t total = 0;
    for (const auto& f : filhos_) total += f->contar_arquivos();
    return total;
}

void Pasta::imprimir_em(int nivel) const {
    fmt::print("    {:>{}}{}/ ({} B, {} arquivos)\n", "", nivel * 2, nome_,
               tamanho(), contar_arquivos());
    for (const auto& f : filhos_) f->imprimir_em(nivel + 1);
}

bool Pasta::remover(const std::string& nome) {
    const auto it = std::find_if(filhos_.begin(), filhos_.end(),
                                 [&nome](const auto& f) { return f->nome() == nome; });
    if (it == filhos_.end()) return false;
    filhos_.erase(it);
    return true;
}

const No* Pasta::encontrar(const std::string& nome) const {
    for (const auto& f : filhos_) {
        if (f->nome() == nome) return f.get();
        // Se o filho for uma pasta, procura dentro dela também
        if (const auto* sub = dynamic_cast<const Pasta*>(f.get())) {
            if (const auto* achado = sub->encontrar(nome)) return achado;
        }
    }
    return nullptr;
}

}  // namespace composite
