#include <QApplication>
#include "mainwindow.hpp"

// ============================================================
// main.cpp — ponto de entrada
//
// Qt exige um QApplication antes de qualquer QWidget.
// O QApplication inicializa o sistema de eventos, o subsistema
// gráfico (X11/Wayland/Win32/Cocoa) e processa argv.
//
// Após show(), o controle passa para exec() que entra no
// loop de eventos do Qt. O game loop da simulação roda dentro
// desse loop via QTimer (não bloqueia a UI).
// ============================================================

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("qt_sfml2");
    app.setApplicationVersion("1.0.0");

    MainWindow window;
    window.show();

    return app.exec();
}
