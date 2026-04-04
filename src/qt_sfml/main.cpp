#include <QApplication>
#include "mainwindow.hpp"

// ============================================================
// PONTO DE ENTRADA
//
// QApplication inicializa o Qt:
//   - sistema de eventos (event loop)
//   - fontes e temas do sistema operacional
//   - suporte a DPI alto
//
// Sem QApplication nenhum widget pode ser criado.
//
// app.exec() entra no event loop — fica aqui até o usuário
// fechar a janela, quando retorna o código de saída.
// ============================================================

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("Academy");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("cpp_academy");

    MainWindow window;
    window.show();

    return app.exec();
}
