#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QLibraryInfo>
#include "MainWindow.h"

static bool loadTranslator(QTranslator &translator, const QString &code)
{
    const QString base = QString("qltotapeman_%1").arg(code);
    const QStringList searchPaths = {
        QString(":/i18n/%1.qm").arg(base),
        QDir(qApp ? qApp->applicationDirPath() : QString()).filePath(QString("translations/%1.qm").arg(base))
    };
    for (const QString &path : searchPaths) {
        if (translator.load(path)) return true;
    }
    return false;
}

static void installPreferredTranslators()
{
    QTranslator *appTranslator = new QTranslator(qApp);
    QTranslator *qtTranslator = new QTranslator(qApp);

    auto pickCodes = [] {
        QList<QString> codes;
        QLocale sys = QLocale::system();
        const QString localeName = sys.name(); // e.g. zh_CN, en_US
        if (localeName.startsWith("zh_", Qt::CaseInsensitive)) {
            if (localeName.compare("zh_TW", Qt::CaseInsensitive) == 0 || sys.script() == QLocale::TraditionalChineseScript)
                codes << "zh_TW" << "zh_CN";
            else
                codes << "zh_CN" << "zh_TW";
        } else if (localeName.startsWith("en", Qt::CaseInsensitive)) {
            codes << "en_US";
        }
        codes << "en_US"; // fallback
        return codes;
    }();

    bool loaded = false;
    for (const QString &code : pickCodes) {
        if (loadTranslator(*appTranslator, code)) {
            qApp->installTranslator(appTranslator);
            loaded = true;
            break;
        }
    }

    // Load Qt base translations if present (optional)
    for (const QString &code : pickCodes) {
        if (qtTranslator->load(QString("qt_%1").arg(code), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            qApp->installTranslator(qtTranslator);
            break;
        }
    }

    if (!loaded) {
        appTranslator->deleteLater();
        qtTranslator->deleteLater();
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    installPreferredTranslators();
    MainWindow w;
    w.show();
    return a.exec();
}
