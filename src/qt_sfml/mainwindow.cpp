#include "mainwindow.hpp"
#include <QVBoxLayout>
#include <QGroupBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Academy — Qt + SFML");
    resize(900, 600);

    // widget central — renderização SFML
    sfml_ = new SFMLWidget(this);
    setCentralWidget(sfml_);

    // painel de controle lateral
    auto* dock   = new QDockWidget("Controles", this);
    auto* panel  = new QWidget(dock);
    auto* layout = new QVBoxLayout(panel);

    // --- grupo velocidade ---
    auto* speed_group  = new QGroupBox("Velocidade");
    auto* speed_layout = new QVBoxLayout(speed_group);

    speed_label_  = new QLabel("200 px/s");
    speed_slider_ = new QSlider(Qt::Horizontal);
    speed_slider_->setRange(50, 600);
    speed_slider_->setValue(200);

    speed_layout->addWidget(speed_label_);
    speed_layout->addWidget(speed_slider_);

    // --- grupo raio ---
    auto* radius_group  = new QGroupBox("Raio");
    auto* radius_layout = new QVBoxLayout(radius_group);

    radius_label_  = new QLabel("30 px");
    radius_slider_ = new QSlider(Qt::Horizontal);
    radius_slider_->setRange(10, 100);
    radius_slider_->setValue(30);

    radius_layout->addWidget(radius_label_);
    radius_layout->addWidget(radius_slider_);

    // --- botão reset ---
    auto* reset_btn = new QPushButton("Resetar posição");

    // --- monta o painel ---
    layout->addWidget(speed_group);
    layout->addWidget(radius_group);
    layout->addWidget(reset_btn);
    layout->addStretch();  // empurra widgets para cima

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // --- connections ---
    connect(speed_slider_,  &QSlider::valueChanged,
            this, &MainWindow::on_speed_changed);

    connect(radius_slider_, &QSlider::valueChanged,
            this, &MainWindow::on_radius_changed);

    connect(reset_btn, &QPushButton::clicked,
            sfml_,     &SFMLWidget::reset);
}

void MainWindow::on_speed_changed(int value) {
    speed_label_->setText(QString("%1 px/s").arg(value));
    sfml_->set_speed(static_cast<float>(value));
}

void MainWindow::on_radius_changed(int value) {
    radius_label_->setText(QString("%1 px").arg(value));
    sfml_->set_radius(static_cast<float>(value));
}