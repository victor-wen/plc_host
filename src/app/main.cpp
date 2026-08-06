#include <QApplication>
#include "app/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("PLC Host");
    app.setApplicationVersion("0.1.0");

    MainWindow window;
    window.show();

    return app.exec();
}
