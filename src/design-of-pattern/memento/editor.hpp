#pragma once
// =============================================================================
//  editor.hpp — o ORIGINADOR e o MEMENTO
//
//  O Memento captura o estado interno de um objeto e permite restaurá-lo
//  depois — SEM violar o encapsulamento.
//
//  Essa última parte é o ponto do padrão, e a que quase todo mundo perde. Um
//  "memento" que expõe os campos publicamente não é um Memento: é um struct
//  de dados, e ele acopla quem guarda ao INTERNO do originador.
//
//  Os três papéis:
//    ORIGINADOR (Editor)   — cria e restaura mementos; é o único que os LÊ
//    MEMENTO    (Estado)   — opaco para todos, exceto o originador
//    ZELADOR    (Historico)— GUARDA os mementos, mas nunca os inspeciona
//
//  Em C++, a opacidade se implementa com membros privados + `friend`.
// =============================================================================
#include <cstddef>
#include <string>

namespace memento {

class Editor {
public:
    // ── O MEMENTO ────────────────────────────────────────────────────────────
    //  Tudo privado. Quem o recebe só pode GUARDAR e DEVOLVER — nem ler,
    //  nem construir, nem modificar.
    class Estado {
    public:
        // A única coisa pública: metadados que não revelam o conteúdo.
        // O zelador precisa disso para exibir o histórico ao usuário.
        const std::string& rotulo() const { return rotulo_; }
        std::size_t        tamanho() const { return conteudo_.size(); }

    private:
        // Só o Editor constrói e lê
        Estado(std::string conteudo, std::size_t cursor, std::string rotulo)
            : conteudo_(std::move(conteudo)), cursor_(cursor),
              rotulo_(std::move(rotulo)) {}

        friend class Editor;

        std::string conteudo_;
        std::size_t cursor_;
        std::string rotulo_;
    };

    // ── O ORIGINADOR ─────────────────────────────────────────────────────────
    void inserir(const std::string& texto);
    void apagar(std::size_t n);
    void mover_cursor(std::size_t pos);

    // Cria um instantâneo
    Estado salvar(std::string rotulo) const {
        return Estado{conteudo_, cursor_, std::move(rotulo)};
    }

    // Restaura — o único que consegue LER o memento
    void restaurar(const Estado& e) {
        conteudo_ = e.conteudo_;
        cursor_   = e.cursor_;
    }

    const std::string& conteudo() const { return conteudo_; }
    std::size_t        cursor() const { return cursor_; }

private:
    std::string conteudo_;
    std::size_t cursor_ = 0;
};

}  // namespace memento
