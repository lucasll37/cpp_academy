#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include "sfml_widget.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void on_speed_changed(int value);
    void on_radius_changed(int value);

private:
    SFMLWidget*  sfml_;
    QSlider*     speed_slider_;
    QSlider*     radius_slider_;
    QLabel*      speed_label_;
    QLabel*      radius_label_;
};