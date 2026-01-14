/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AboutDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <QSysInfo>
#include <QThread>
#include <QScreen>
#include <QGuiApplication>
#include <QLibraryInfo>

namespace qltfs {
namespace app {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

void AboutDialog::setupUi()
{
    setWindowTitle(tr("About QLTOTapeMan"));
    setMinimumSize(500, 400);
    resize(550, 450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    createAboutTab();
    createLicenseTab();
    createCreditsTab();
    createSystemTab();
    mainLayout->addWidget(m_tabWidget);

    // Close button
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setDefault(true);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void AboutDialog::createAboutTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(15);

    // Logo placeholder
    m_logoLabel = new QLabel(tab);
    m_logoLabel->setPixmap(QPixmap(":/icons/app.png").scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_logoLabel);

    // Title
    m_titleLabel = new QLabel("QLTOTapeMan", tab);
    m_titleLabel->setStyleSheet("font-size: 24pt; font-weight: bold;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_titleLabel);

    // Version
    QString version = QCoreApplication::applicationVersion();
    if (version.isEmpty()) {
        version = "1.0.0";
    }
    m_versionLabel = new QLabel(tr("Version %1").arg(version), tab);
    m_versionLabel->setStyleSheet("font-size: 12pt;");
    m_versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_versionLabel);

    // Description
    QLabel *descLabel = new QLabel(tr("Qt-based LTO Tape Manager with LTFS Support"), tab);
    descLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(descLabel);

    layout->addStretch();

    // Copyright
    m_copyrightLabel = new QLabel(tab);
    m_copyrightLabel->setText(
        "Copyright © 2026 Jeffrey ZHU\n"
        "zhxsh1225@gmail.com\n\n"
        "https://github.com/Gypsop/QLTOTapeMan"
    );
    m_copyrightLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_copyrightLabel);

    layout->addStretch();

    m_tabWidget->addTab(tab, tr("About"));
}

void AboutDialog::createLicenseTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    m_licenseText = new QTextEdit(tab);
    m_licenseText->setReadOnly(true);
    m_licenseText->setPlainText(
        "Apache License\n"
        "Version 2.0, January 2004\n"
        "http://www.apache.org/licenses/\n\n"
        "TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION\n\n"
        "1. Definitions.\n\n"
        "\"License\" shall mean the terms and conditions for use, reproduction,\n"
        "and distribution as defined by Sections 1 through 9 of this document.\n\n"
        "\"Licensor\" shall mean the copyright owner or entity authorized by\n"
        "the copyright owner that is granting the License.\n\n"
        "\"Legal Entity\" shall mean the union of the acting entity and all\n"
        "other entities that control, are controlled by, or are under common\n"
        "control with that entity...\n\n"
        "[Full license text available at http://www.apache.org/licenses/LICENSE-2.0]\n\n"
        "Unless required by applicable law or agreed to in writing, software\n"
        "distributed under the License is distributed on an \"AS IS\" BASIS,\n"
        "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
        "See the License for the specific language governing permissions and\n"
        "limitations under the License."
    );
    layout->addWidget(m_licenseText);

    m_tabWidget->addTab(tab, tr("License"));
}

void AboutDialog::createCreditsTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    m_creditsText = new QTextEdit(tab);
    m_creditsText->setReadOnly(true);
    m_creditsText->setHtml(
        "<h3>Development</h3>"
        "<p>Jeffrey ZHU &lt;zhxsh1225@gmail.com&gt;</p>"
        "<h3>Based on</h3>"
        "<p>LTFSCopyGUI - Original VB.NET implementation</p>"
        "<h3>Libraries Used</h3>"
        "<ul>"
        "<li><b>Qt Framework</b> - https://www.qt.io/</li>"
        "<li><b>LTFS</b> - Linear Tape File System</li>"
        "</ul>"
        "<h3>Acknowledgments</h3>"
        "<p>Thanks to all contributors and users of this project.</p>"
        "<p>Special thanks to the LTFS community and IBM for the LTFS specification.</p>"
        "<h3>Translations</h3>"
        "<ul>"
        "<li>English - Jeffrey ZHU</li>"
        "<li>简体中文 - Jeffrey ZHU</li>"
        "<li>繁體中文 - Jeffrey ZHU</li>"
        "</ul>"
    );
    layout->addWidget(m_creditsText);

    m_tabWidget->addTab(tab, tr("Credits"));
}

void AboutDialog::createSystemTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    m_systemText = new QTextEdit(tab);
    m_systemText->setReadOnly(true);
    m_systemText->setPlainText(getSystemInfo());
    layout->addWidget(m_systemText);

    m_tabWidget->addTab(tab, tr("System"));
}

QString AboutDialog::getSystemInfo() const
{
    QString info;

    info += "=== Application ===\n";
    info += QString("Name: %1\n").arg(QCoreApplication::applicationName());
    info += QString("Version: %1\n").arg(QCoreApplication::applicationVersion().isEmpty() 
                                          ? "1.0.0" : QCoreApplication::applicationVersion());
    info += QString("Organization: %1\n").arg(QCoreApplication::organizationName());
    info += "\n";

    info += "=== Qt ===\n";
    info += QString("Qt Version: %1\n").arg(qVersion());
    info += QString("Qt Build: %1\n").arg(QLibraryInfo::build());
    info += "\n";

    info += "=== Operating System ===\n";
    info += QString("OS: %1\n").arg(QSysInfo::prettyProductName());
    info += QString("Kernel: %1 %2\n").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
    info += QString("Architecture: %1\n").arg(QSysInfo::currentCpuArchitecture());
    info += "\n";

    info += "=== Hardware ===\n";
    info += QString("CPU Threads: %1\n").arg(QThread::idealThreadCount());
    
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        info += QString("Primary Screen: %1x%2 @ %3 DPI\n")
            .arg(screen->size().width())
            .arg(screen->size().height())
            .arg(screen->logicalDotsPerInch());
    }
    info += "\n";

    info += "=== Build ===\n";
    info += QString("Compiler: %1\n").arg(
#if defined(__clang__)
        QString("Clang %1.%2.%3").arg(__clang_major__).arg(__clang_minor__).arg(__clang_patchlevel__)
#elif defined(__GNUC__)
        QString("GCC %1.%2.%3").arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
        QString("MSVC %1").arg(_MSC_VER)
#else
        QString("Unknown")
#endif
    );
    info += QString("Build Date: %1 %2\n").arg(__DATE__, __TIME__);

    return info;
}

} // namespace app
} // namespace qltfs
