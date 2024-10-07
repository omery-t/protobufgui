#include <QApplication>
#include "ProtobufGUI.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ProtobufGUI gui;
    gui.show();
    return app.exec();
}