#pragma once
#include <QMainWindow>
#include <QColorDialog>
#include "sfml_widget.hpp"

// ============================================================
// ui_mainwindow.h é gerado automaticamente pelo uic
// a partir do mainwindow.ui que criamos no Qt Designer.
//
// O Meson chama o uic via qt6.compile_ui() no meson.build.
// Nunca edite ui_mainwindow.h diretamente — ele é sobrescrito
// a cada build. Edite sempre o .ui.
// ============================================================
#include "ui_mainwindow.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    // nomeados no padrão on_<objectName>_<signal>
    // o Qt conecta automaticamente via QMetaObject::connectSlotsByName
    void on_sliderSpeed_valueChanged(int value);
    void on_sliderRadius_valueChanged(int value);
    void on_btnReset_clicked();
    void on_btnColor_clicked();
    void on_actionReset_triggered();
    void on_actionQuit_triggered();

private:
    // ui_ contém todos os widgets definidos no .ui
    // gerado pelo uic: labelSpeed, sliderSpeed, sliderRadius, etc.
    Ui::MainWindow ui_;

    // o widget de renderização SFML
    // inserido programaticamente no centralwidget
    SFMLWidget* sfml_;
};
