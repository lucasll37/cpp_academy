#pragma once

// ============================================================
// MainWindow
//
// Responsabilidade única: UI Qt.
//   - Monta o layout: SFMLWidget à esquerda, dock à direita
//   - Conecta signals/slots dos controles ao SFMLWidget
//   - Atualiza labels de status (contagem de objetos, FPS)
//
// NÃO conhece Bullet3, NÃO conhece SFML.
// Comunica-se com SFMLWidget exclusivamente via métodos públicos.
// ============================================================

#include <QMainWindow>
#include <QTimer>
#include "ui_mainwindow.h"
#include "sfml_widget.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    // ── Botões Adicionar ──────────────────────────────────────────────────
    void on_btnAddSphere_clicked();
    void on_btnAddBox_clicked();
    void on_btnAddCylinder_clicked();

    // ── Ações de menu (atalhos de teclado) ───────────────────────────────
    void on_actionAddSphere_triggered();
    void on_actionAddBox_triggered();
    void on_actionAddCylinder_triggered();
    void on_actionPausar_triggered(bool checked);
    void on_actionReiniciar_triggered();
    void on_actionSair_triggered();

    // ── Física ───────────────────────────────────────────────────────────
    void on_comboGravity_currentIndexChanged(int index);
    void on_sliderRestitution_valueChanged(int value);

    // ── Ações diretas ─────────────────────────────────────────────────────
    void on_btnExplode_clicked();
    void on_btnPause_toggled(bool checked);
    void on_btnClear_clicked();

    // ── Timer de status (atualiza labels de FPS e contagem) ───────────────
    void update_status();

private:
    Ui::MainWindow ui_;
    SFMLWidget*    sfml_;

    // Timer separado para atualizar os labels de status a ~4Hz
    // (não precisa ser a 60fps)
    QTimer status_timer_;

    // Contagem de frames para cálculo de FPS
    int    frame_count_ = 0;
    qint64 last_fps_ms_ = 0;
};
