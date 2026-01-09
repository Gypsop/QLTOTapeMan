#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QLabel splash(QObject::tr("QLTOTapeMan initialized (skeleton build)"));
    splash.show();
    return app.exec();
}
