/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QDir>
#include <QStyleFactory>
#include <QMessageBox>

#include "gui/MainWindow.h"

int main(int argc, char *argv[])
{
    // Enable high DPI scaling
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // Set application info
    app.setApplicationName(QStringLiteral("QLTOTapeMan"));
    app.setApplicationDisplayName(QStringLiteral("QLTOTapeMan - LTO Tape Manager"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("JeffreyZHU"));
    app.setOrganizationDomain(QStringLiteral("github.com/Gypsop"));

    // Set application style
#ifdef Q_OS_WIN
    // Use Fusion style on Windows for consistent look
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
#endif

    // Load Qt translations
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(),
                          QStringLiteral("qt"),
                          QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // Load application translations
    QTranslator appTranslator;
    QStringList searchPaths;

    // Look for translations in various locations
    searchPaths << app.applicationDirPath() + QStringLiteral("/translations");
    searchPaths << app.applicationDirPath() + QStringLiteral("/../share/QLTOTapeMan/translations");
    searchPaths << QStringLiteral(":/translations");

    for (const QString &path : searchPaths) {
        if (appTranslator.load(QLocale::system(),
                               QStringLiteral("QLTOTapeMan"),
                               QStringLiteral("_"),
                               path)) {
            app.installTranslator(&appTranslator);
            break;
        }
    }

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
