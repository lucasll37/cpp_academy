#include "mainwindow.hpp"
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // setupUi lê o ui_mainwindow.h gerado pelo uic e
    // instancia todos os widgets definidos no .ui:
    // menubar, statusbar, dockControls, sliders, labels, botões
    ui_.setupUi(this);

    // o SFMLWidget não pode ser definido no Qt Designer
    // porque é uma classe customizada que herda QWidget
    // inserimos programaticamente no centralwidget
    sfml_ = new SFMLWidget(ui_.centralwidget);

    auto* layout = new QHBoxLayout(ui_.centralwidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(sfml_);

    // connectSlotsByName conecta automaticamente os slots
    // nomeados no padrão on_<objectName>_<signal>
    // ex: on_sliderSpeed_valueChanged → sliderSpeed::valueChanged
    // funciona porque setupUi já nomeou os objetos corretamente
    QMetaObject::connectSlotsByName(this);

    // status bar com instrução inicial
    statusBar()->showMessage("Pronto — use os controles para ajustar a simulação");
}

void MainWindow::on_sliderSpeed_valueChanged(int value) {
    ui_.labelSpeed->setText(QString("%1 px/s").arg(value));
    sfml_->set_speed(static_cast<float>(value));
}

void MainWindow::on_sliderRadius_valueChanged(int value) {
    ui_.labelRadius->setText(QString("%1 px").arg(value));
    sfml_->set_radius(static_cast<float>(value));
}

void MainWindow::on_btnReset_clicked() {
    sfml_->reset();
    statusBar()->showMessage("Posição resetada");
}

void MainWindow::on_btnColor_clicked() {
    // QColorDialog: diálogo nativo de seleção de cor
    QColor color = QColorDialog::getColor(
        QColor(100, 200, 255),  // cor inicial
        this,
        "Escolher cor da bola"
    );

    if (color.isValid()) {
        sfml_->set_color(sf::Color(
            static_cast<sf::Uint8>(color.red()),
            static_cast<sf::Uint8>(color.green()),
            static_cast<sf::Uint8>(color.blue())
        ));
        statusBar()->showMessage(
            QString("Cor alterada para %1").arg(color.name())
        );
    }
}

void MainWindow::on_actionReset_triggered() {
    // menu Simulation → Resetar faz a mesma coisa que o botão
    sfml_->reset();
    statusBar()->showMessage("Posição resetada via menu");
}

void MainWindow::on_actionQuit_triggered() {
    close();
}
