#include "mainwindow.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QElapsedTimer>
#include <QThread>
#include <QString>
#include <QScrollBar>

// ============================================================
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Mandelbrot Zoom — mutex + threads demo");
    resize(900, 700);

    // ── Widget central ──────────────────────────────────────
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* vlay = new QVBoxLayout(central);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(4);

    // ── Toolbar ─────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    vlay->addLayout(toolbar);

    // Threads
    toolbar->addWidget(new QLabel("Threads:"));
    thread_slider_ = new QSlider(Qt::Horizontal);
    int hw_threads = std::max(1, (int)QThread::idealThreadCount());
    thread_slider_->setRange(1, std::max(16, hw_threads));
    thread_slider_->setValue(hw_threads);
    thread_slider_->setFixedWidth(140);
    thread_slider_->setToolTip("Número de worker threads");
    toolbar->addWidget(thread_slider_);

    auto* thread_label = new QLabel(QString::number(hw_threads));
    thread_label->setFixedWidth(30);
    connect(thread_slider_, &QSlider::valueChanged, [thread_label](int v){
        thread_label->setText(QString::number(v));
    });
    toolbar->addWidget(thread_label);

    toolbar->addSpacing(20);

    // Iterações
    toolbar->addWidget(new QLabel("Iter:"));
    iter_spin_ = new QSpinBox;
    iter_spin_->setRange(32, 4096);
    iter_spin_->setValue(256);
    iter_spin_->setSingleStep(64);
    iter_spin_->setFixedWidth(80);
    iter_spin_->setToolTip("Máximo de iterações");
    toolbar->addWidget(iter_spin_);

    toolbar->addSpacing(20);

    // Reset
    auto* btn_reset = new QPushButton("Reset");
    btn_reset->setToolTip("Ctrl+R — reseta viewport");
    toolbar->addWidget(btn_reset);

    toolbar->addStretch();

    // ── Canvas ───────────────────────────────────────────────
    canvas_ = new QLabel;
    canvas_->setAlignment(Qt::AlignCenter);
    canvas_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    canvas_->setStyleSheet("background: #000;");
    vlay->addWidget(canvas_, 1);

    // ── Status bar ───────────────────────────────────────────
    statusBar()->showMessage("Pronto");

    // ── Renderer ─────────────────────────────────────────────
    int W = canvas_->width()  > 0 ? canvas_->width()  : 880;
    int H = canvas_->height() > 0 ? canvas_->height() : 640;
    renderer_ = std::make_unique<MandelbrotRenderer>(
        W, H, hw_threads, iter_spin_->value()
    );

    // ── Timer para coalescer eventos rápidos (scroll, resize) ─
    render_timer_ = new QTimer(this);
    render_timer_->setSingleShot(true);
    render_timer_->setInterval(30); // ms
    connect(render_timer_, &QTimer::timeout, this, &MainWindow::do_render);

    // ── Conexões ─────────────────────────────────────────────
    connect(thread_slider_, &QSlider::valueChanged,
            this, &MainWindow::schedule_render);
    connect(iter_spin_, qOverload<int>(&QSpinBox::valueChanged),
            this, &MainWindow::schedule_render);
    connect(btn_reset, &QPushButton::clicked, [this](){
        vp_ = Viewport{};
        schedule_render();
    });

    // Render inicial
    do_render();
}

// ============================================================
void MainWindow::schedule_render() {
    render_timer_->start(); // reinicia o timer — coalesce eventos
}

void MainWindow::do_render() {
    // Sincroniza parâmetros do renderer com os controles
    renderer_->set_threads(thread_slider_->value());
    renderer_->set_max_iter(iter_spin_->value());

    // Redimensiona se o canvas mudou de tamanho
    int W = canvas_->width();
    int H = canvas_->height();
    if (W < 8 || H < 8) return;

    if (renderer_->width() != W || renderer_->height() != H)
        renderer_->resize(W, H);

    // ── Mede tempo de render ──────────────────────────────────
    QElapsedTimer et;
    et.start();

    const auto& px = renderer_->render(vp_);

    double elapsed_ms = et.elapsed();

    // ── Converte buffer RGBA → QImage ─────────────────────────
    // QImage::Format_RGBA8888 espera R,G,B,A na memória
    QImage img(px.data(), W, H, W * 4, QImage::Format_RGBA8888);
    canvas_->setPixmap(QPixmap::fromImage(img));

    // ── Atualiza status bar ───────────────────────────────────
    QString status = QString(
        "%1 thread(s)  |  %2 iter  |  "
        "centro (%.6f, %.6f)  |  escala %.2e  |  %3 ms"
    ).arg(renderer_->threads())
     .arg(renderer_->max_iter())
     .arg(elapsed_ms, 0, 'f', 1);

    // Monta manualmente para incluir doubles com boa precisão
    statusBar()->showMessage(
        QString("%1 thread(s)  |  %2 iter  |  "
                "cx=%3  cy=%4  |  scale=%5  |  %6 ms")
            .arg(renderer_->threads())
            .arg(renderer_->max_iter())
            .arg(vp_.cx,    0, 'f', 8)
            .arg(vp_.cy,    0, 'f', 8)
            .arg(vp_.scale, 0, 'e', 3)
            .arg(elapsed_ms, 0, 'f', 1)
    );
}

// ── Resize ───────────────────────────────────────────────────
void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    schedule_render();
}

// ── Zoom com scroll do mouse ──────────────────────────────────
void MainWindow::wheelEvent(QWheelEvent* e) {
    double factor = (e->angleDelta().y() > 0) ? 0.75 : 1.333;

    // Mapeia posição do cursor para o plano complexo ANTES do zoom
    QPoint pos = canvas_->mapFromGlobal(e->globalPosition().toPoint());
    QPointF c  = pixel_to_complex(pos);

    // Aplica zoom mantendo o ponto sob o cursor fixo
    vp_.scale *= factor;
    vp_.cx = c.x() + (vp_.cx - c.x()) * factor;
    vp_.cy = c.y() + (vp_.cy - c.y()) * factor;

    schedule_render();
    e->accept();
}

// ── Pan com drag ──────────────────────────────────────────────
void MainWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        dragging_  = true;
        drag_start_ = e->pos();
        drag_cx0_   = vp_.cx;
        drag_cy0_   = vp_.cy;
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent* e) {
    if (!dragging_) return;
    QPoint delta = e->pos() - drag_start_;
    double aspect = (double)renderer_->width() / renderer_->height();
    double px_to_cx = vp_.scale / renderer_->width();
    double px_to_cy = vp_.scale / renderer_->width() / aspect;
    vp_.cx = drag_cx0_ - delta.x() * px_to_cx;
    vp_.cy = drag_cy0_ - delta.y() * px_to_cy;
    schedule_render();
}

void MainWindow::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) dragging_ = false;
}

// ── Reset via teclado ─────────────────────────────────────────
void MainWindow::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_R && e->modifiers() & Qt::ControlModifier) {
        vp_ = Viewport{};
        schedule_render();
    }
    QMainWindow::keyPressEvent(e);
}

// ── Conversão pixel → complexo ────────────────────────────────
QPointF MainWindow::pixel_to_complex(QPoint p) const {
    int W = renderer_->width(), H = renderer_->height();
    double aspect = (double)W / H;
    double cr = vp_.cx + (p.x() / (double)(W-1) - 0.5) * vp_.scale;
    double ci = vp_.cy + (p.y() / (double)(H-1) - 0.5) * vp_.scale / aspect;
    return {cr, ci};
}
