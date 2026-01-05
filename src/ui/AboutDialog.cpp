#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include "../utils/SettingsManager.h"
#include "../Version.h"
#include <QSysInfo>
#include <QProcess>
#include <QDebug>

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->lblVersion->setText(QString("Version %1").arg(APP_VERSION));
    ui->lblOrg->setText(APP_ORG);
    collectSystemInfo();
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::collectSystemInfo()
{
    // --- Tab 1: System & Tools ---
    QString html = "<style>"
                   "table { border-collapse: collapse; width: 100%; }"
                   "td, th { border: 1px solid #ddd; padding: 8px; }"
                   "tr:nth-child(even){background-color: #f2f2f2;}"
                   "th { padding-top: 12px; padding-bottom: 12px; text-align: left; background-color: #04AA6D; color: white; }"
                   "</style>";
    
    html += "<h3>System Information</h3>";
    html += "<table>";
    html += QString("<tr><td><b>OS</b></td><td>%1</td></tr>").arg(getOsInfo());
    html += QString("<tr><td><b>Qt Version</b></td><td>%1</td></tr>").arg(qVersion());
    html += QString("<tr><td><b>Architecture</b></td><td>%1</td></tr>").arg(QSysInfo::currentCpuArchitecture());
    html += "</table>";
    
    html += "<h3>LTFS Tools</h3>";
    html += "<table>";
    html += getLtfsVersion();
    html += "</table>";
    
    ui->textSystemInfo->setHtml(html);
    
    // --- Tab 2: Hardware ---
    ui->textHardwareInfo->setPlainText("Scanning hardware...\n");
    // We can do this async later, but for now sync is fine as it's a dialog
    ui->textHardwareInfo->setPlainText(getHardwareInfo());
}

QString AboutDialog::getOsInfo()
{
    return QSysInfo::prettyProductName();
}

QString AboutDialog::getLtfsVersion()
{
    QString rows;
    QString ltfsPath = SettingsManager::instance().ltfsBinaryPath();
    if (ltfsPath.isEmpty()) ltfsPath = "ltfs";
    
    QProcess process;
    process.start(ltfsPath, QStringList() << "--version");
    if (process.waitForFinished(1000)) {
        QString output = process.readAllStandardOutput();
        rows += QString("<tr><td><b>ltfs</b></td><td>%1</td></tr>").arg(output.split('\n').first());
    } else {
        rows += "<tr><td><b>ltfs</b></td><td><font color='red'>Not found</font></td></tr>";
    }
    
    QString mkltfsPath = SettingsManager::instance().mkltfsBinaryPath();
    if (mkltfsPath.isEmpty()) mkltfsPath = "mkltfs";
    
    process.start(mkltfsPath, QStringList() << "--version");
    if (process.waitForFinished(1000)) {
        QString output = process.readAllStandardOutput();
        rows += QString("<tr><td><b>mkltfs</b></td><td>%1</td></tr>").arg(output.split('\n').first());
    } else {
        rows += "<tr><td><b>mkltfs</b></td><td><font color='red'>Not found</font></td></tr>";
    }
    
    return rows;
}

QString AboutDialog::getHardwareInfo()
{
    QString info;
    QProcess process;
    
#ifdef Q_OS_MAC
    // macOS: system_profiler SPSASDataType
    info += "--- SAS Devices ---\n";
    process.start("system_profiler", QStringList() << "SPSASDataType");
    process.waitForFinished(3000);
    info += process.readAllStandardOutput();
    
    info += "\n--- Thunderbolt Devices ---\n";
    process.start("system_profiler", QStringList() << "SPThunderboltDataType");
    process.waitForFinished(3000);
    info += process.readAllStandardOutput();
#elif defined(Q_OS_WIN)
    // Windows: wmic
    info += "--- Tape Drives ---\n";
    process.start("wmic", QStringList() << "path" << "Win32_TapeDrive" << "get" << "Name,Description,DeviceID,Status");
    process.waitForFinished(3000);
    info += process.readAllStandardOutput();
    
    info += "\n--- SCSI Controllers ---\n";
    process.start("wmic", QStringList() << "path" << "Win32_SCSIController" << "get" << "Name,Manufacturer");
    process.waitForFinished(3000);
    info += process.readAllStandardOutput();
#else
    // Linux
    process.start("lsscsi", QStringList() << "-g");
    if (process.waitForFinished(1000)) {
        info += process.readAllStandardOutput();
    } else {
        // Fallback to lspci for controllers
        process.start("lspci");
        process.waitForFinished(1000);
        QString lspciOut = process.readAllStandardOutput();
        QStringList lines = lspciOut.split('\n');
        for(const QString &line : lines) {
            if (line.contains("SAS", Qt::CaseInsensitive) || line.contains("SCSI", Qt::CaseInsensitive)) {
                info += line + "\n";
            }
        }
    }
#endif

    return info;
}
