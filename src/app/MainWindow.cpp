#include "MainWindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("PLC Host v0.1.0");
    resize(1280, 800);
}

MainWindow::~MainWindow() = default;
