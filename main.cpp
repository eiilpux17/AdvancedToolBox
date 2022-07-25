#include "widget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    a.setStyleSheet("file:///:/style.qss");
    Widget w;
    w.show();
    return a.exec();
}
