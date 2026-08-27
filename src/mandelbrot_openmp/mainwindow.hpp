#pragma once

// ============================================================
// MainWindow — janela principal do Mandelbrot Zoom
//
// Layout (sem .ui — tudo programático para não precisar de uic):
//
//   ┌──────────────────────────────────────────┐
//   │  [Threads: --[slider]++]  [Iter: --[s]++]│  ← toolbar
//   │  [Status: "8 threads | 256 iter | 0.42s"]│
//   ├──────────────────────────────────────────┤
//   │                                          │
//   │         QLabel (imagem Mandelbrot)        │
//   │                                          │
//   └──────────────────────────────────────────┘
//
// Interação:
//   Scroll → zoom centrado no cursor
//   Drag   → pan
//   Ctrl+R → reset viewport
// ============================================================

#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QImage>
#include <QTimer>
#include <QPoint>
#include <memory>
#include "mandelbrot_renderer.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void wheelEvent(QWheelEvent* e)   override;
    void mousePressEvent(QMouseEvent* e)   override;
    void mouseMoveEvent(QMouseEvent* e)    override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void schedule_render();   // agenda re-render via timer (coalesce eventos rápidos)
    void do_render();         // executa o render de fato

private:
    void rebuild_image();     // converte pixels_ → QImage → QLabel
    QPointF pixel_to_complex(QPoint p) const;

    // --- widgets ---
    QLabel*   canvas_;
    QSlider*  thread_slider_;
    QSpinBox* iter_spin_;

    // --- renderer ---
    std::unique_ptr<MandelbrotRenderer> renderer_;
    Viewport vp_;

    // --- navegação ---
    bool   dragging_  = false;
    QPoint drag_start_;
    double drag_cx0_, drag_cy0_;

    // --- render agendado ---
    QTimer* render_timer_;
};
