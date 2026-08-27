#pragma once
// =============================================================================
//  historico.hpp — o ZELADOR
//
//  Ele guarda os mementos e nunca olha dentro deles. Repare no que ele NÃO
//  consegue fazer, e que o compilador impede:
//
//      Editor::Estado e = ...;
//      e.conteudo_;                 // ✗ privado
//      Editor::Estado{"x", 0, "y"}; // ✗ construtor privado
//
//  Ele só sabe empilhar, desempilhar e mostrar o rótulo.
// =============================================================================
#include "editor.hpp"

#include <string>
#include <vector>

namespace memento {

class Historico {
public:
    void registrar(Editor::Estado e) {
        // Ao gravar um estado novo, o "refazer" perde o sentido
        futuros_.clear();
        passados_.push_back(std::move(e));
    }

    bool pode_desfazer() const { return !passados_.empty(); }
    bool pode_refazer() const  { return !futuros_.empty(); }

    // Devolve o estado anterior, guardando o atual para o "refazer"
    bool desfazer(Editor& editor);
    bool refazer(Editor& editor);

    std::vector<std::string> rotulos() const;

    std::size_t tamanho() const { return passados_.size(); }
    std::size_t bytes() const;

    // Limita o histórico, descartando os mais antigos
    void definir_limite(std::size_t n) { limite_ = n; aparar(); }

private:
    void aparar();

    std::vector<Editor::Estado> passados_;
    std::vector<Editor::Estado> futuros_;
    std::size_t                 limite_ = 0;   // 0 = sem limite
};

}  // namespace memento
