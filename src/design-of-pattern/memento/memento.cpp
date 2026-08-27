#include "editor.hpp"
#include "historico.hpp"

#include <fmt/core.h>

#include <algorithm>

namespace memento {

// ── Editor ───────────────────────────────────────────────────────────────────
void Editor::inserir(const std::string& texto) {
    cursor_ = std::min(cursor_, conteudo_.size());
    conteudo_.insert(cursor_, texto);
    cursor_ += texto.size();
}

void Editor::apagar(std::size_t n) {
    const auto quantos = std::min(n, cursor_);
    conteudo_.erase(cursor_ - quantos, quantos);
    cursor_ -= quantos;
}

void Editor::mover_cursor(std::size_t pos) {
    cursor_ = std::min(pos, conteudo_.size());
}

// ── Historico ────────────────────────────────────────────────────────────────
bool Historico::desfazer(Editor& editor) {
    if (passados_.empty()) return false;

    // Guarda o estado ATUAL para permitir refazer
    futuros_.push_back(editor.salvar("antes-de-desfazer"));

    editor.restaurar(passados_.back());
    passados_.pop_back();
    return true;
}

bool Historico::refazer(Editor& editor) {
    if (futuros_.empty()) return false;

    passados_.push_back(editor.salvar("antes-de-refazer"));

    editor.restaurar(futuros_.back());
    futuros_.pop_back();
    return true;
}

std::vector<std::string> Historico::rotulos() const {
    std::vector<std::string> r;
    r.reserve(passados_.size());
    // O zelador só consegue ler o RÓTULO — o conteúdo é opaco para ele
    for (const auto& e : passados_) {
        r.push_back(fmt::format("{} ({} B)", e.rotulo(), e.tamanho()));
    }
    return r;
}

std::size_t Historico::bytes() const {
    std::size_t t = 0;
    for (const auto& e : passados_) t += e.tamanho();
    for (const auto& e : futuros_)  t += e.tamanho();
    return t;
}

void Historico::aparar() {
    if (limite_ == 0 || passados_.size() <= limite_) return;
    const auto excesso = passados_.size() - limite_;
    passados_.erase(passados_.begin(),
                    passados_.begin() + static_cast<std::ptrdiff_t>(excesso));
}

}  // namespace memento
