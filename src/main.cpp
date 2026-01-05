#include "MainWindow.h"
#include "Version.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName(APP_NAME);
    a.setApplicationVersion(APP_VERSION);
    a.setOrganizationName(APP_ORG);

    MainWindow w;
    w.show();
    return a.exec();
}
