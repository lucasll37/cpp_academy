#include "mainwindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QElapsedTimer>
#include <QDateTime>

// Valores de gravidade mapeados pelos índices do comboGravity
static constexpr float GRAVITY_VALUES[] = {
    -9.82f,   // Normal
     0.0f,    // Zero-G
    -2.0f,    // Fraca
    -1.62f,   // Lunar
   -20.0f,    // Pesada
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    ui_.setupUi(this);

    // ── Cria o SFMLWidget e insere no centralwidget ───────────────────────
    // O centralwidget gerado pelo uic está vazio — damos um layout a ele
    // e inserimos o SFMLWidget como único filho.
    sfml_ = new SFMLWidget(this);
    auto* central_layout = new QHBoxLayout(ui_.centralwidget);
    central_layout->setContentsMargins(0, 0, 0, 0);
    central_layout->addWidget(sfml_);

    // ── Timer de status ───────────────────────────────────────────────────
    // Atualiza os labels de contagem e FPS a cada 250ms
    connect(&status_timer_, &QTimer::timeout, this, &MainWindow::update_status);
    status_timer_.start(250);
    last_fps_ms_ = QDateTime::currentMSecsSinceEpoch();

    // ── Conecta o tick do SFML ao contador de FPS ─────────────────────────
    // O QTimer interno do SFMLWidget não é acessível diretamente, mas
    // podemos usar um segundo timer a 60fps para contar frames.
    auto* fps_timer = new QTimer(this);
    connect(fps_timer, &QTimer::timeout, this, [this](){
        ++frame_count_;
    });
    fps_timer->start(1000 / 60);

    setWindowTitle("Qt + SFML + Bullet3 — Simulação 3D");
    resize(1100, 700);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::update_status() {
    ui_.labelObjCount->setText(
        QString("Objetos: %1").arg(sfml_->object_count()));

    qint64 now  = QDateTime::currentMSecsSinceEpoch();
    qint64 diff = now - last_fps_ms_;
    if (diff > 0) {
        float fps = frame_count_ * 1000.f / diff;
        ui_.labelFPS->setText(QString("FPS: %1").arg((int)fps));
    }
    frame_count_ = 0;
    last_fps_ms_ = now;
}

// ─────────────────────────────────────────────────────────────────────────────
// Botões Adicionar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::on_btnAddSphere_clicked()   { sfml_->spawn_object(ShapeType::Sphere); }
void MainWindow::on_btnAddBox_clicked()      { sfml_->spawn_object(ShapeType::Box); }
void MainWindow::on_btnAddCylinder_clicked() { sfml_->spawn_object(ShapeType::Cylinder); }

// ─────────────────────────────────────────────────────────────────────────────
// Ações de menu (espelham os botões com atalhos de teclado)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::on_actionAddSphere_triggered()   { sfml_->spawn_object(ShapeType::Sphere); }
void MainWindow::on_actionAddBox_triggered()      { sfml_->spawn_object(ShapeType::Box); }
void MainWindow::on_actionAddCylinder_triggered() { sfml_->spawn_object(ShapeType::Cylinder); }

void MainWindow::on_actionPausar_triggered(bool checked) {
    sfml_->set_paused(checked);
    ui_.btnPause->setChecked(checked);
}

void MainWindow::on_actionReiniciar_triggered() {
    sfml_->clear_objects();
    for (int i = 0; i < 6; ++i) sfml_->spawn_object(ShapeType::Sphere);
    for (int i = 0; i < 4; ++i) sfml_->spawn_object(ShapeType::Box);
    for (int i = 0; i < 2; ++i) sfml_->spawn_object(ShapeType::Cylinder);
}

void MainWindow::on_actionSair_triggered() { close(); }

// ─────────────────────────────────────────────────────────────────────────────
// Física
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::on_comboGravity_currentIndexChanged(int index) {
    if (index >= 0 && index < (int)(sizeof(GRAVITY_VALUES)/sizeof(float)))
        sfml_->set_gravity(GRAVITY_VALUES[index]);
}

void MainWindow::on_sliderRestitution_valueChanged(int value) {
    float r = value / 100.f;
    sfml_->set_restitution(r);
    ui_.labelRestitutionVal->setText(QString::number(r, 'f', 2));
}

// ─────────────────────────────────────────────────────────────────────────────
// Ações diretas
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::on_btnExplode_clicked() { sfml_->explode(); }
void MainWindow::on_btnClear_clicked()   { sfml_->clear_objects(); }

void MainWindow::on_btnPause_toggled(bool checked) {
    sfml_->set_paused(checked);
    ui_.btnPause->setText(checked ? "▶ Continuar" : "⏸ Pausar");
    ui_.actionPausar->setChecked(checked);
}
