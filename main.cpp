#include "main_window.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    main_window w;
    w.setWindowTitle("Custom Vector Network Analyzer");
    w.show();
    return a.exec();
}
