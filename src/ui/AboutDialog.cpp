#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include "../utils/SettingsManager.h"
#include "../Version.h"
#include <QSysInfo>
#include <QProcess>
#include <QDebug>
#include <QHeaderView>
#include <QGroupBox>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->lblVersion->setText(QString("Version %1").arg(APP_VERSION));
    ui->lblOrg->setText(APP_ORG);
    
    setupUiCustom();
    collectSystemInfo();
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::setupUiCustom()
{
    // Remove the old text browser from the layout and delete it
    if (ui->textSystemInfo) {
        ui->tabSystem->layout()->removeWidget(ui->textSystemInfo);
        delete ui->textSystemInfo;
        ui->textSystemInfo = nullptr;
    }

    // Create tables within groups
    m_sysTable = createGroupTable("System Information", ui->tabSystem);
    m_ltfsTable = createGroupTable("LTFS Tools", ui->tabSystem);
    
    // Add spacer to push everything up
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(ui->tabSystem->layout());
    if (layout) {
        layout->addStretch();
    }
}

QTableWidget* AboutDialog::createGroupTable(const QString &title, QWidget *parentLayoutWidget)
{
    QGroupBox *groupBox = new QGroupBox(title, parentLayoutWidget);
    QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);
    
    QTableWidget *table = new QTableWidget(groupBox);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Item", "Value"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setStyleSheet("QTableWidget { border: none; background-color: transparent; }");
    // Set a fixed height based on rows later, or minimum height
    table->setMinimumHeight(100); 
    
    groupLayout->addWidget(table);
    parentLayoutWidget->layout()->addWidget(groupBox);
    
    return table;
}

void AboutDialog::addRowToTable(QTableWidget *table, const QString &key, const QString &value, bool isError)
{
    int row = table->rowCount();
    table->insertRow(row);
    
    QTableWidgetItem *itemKey = new QTableWidgetItem(key);
    QTableWidgetItem *itemValue = new QTableWidgetItem(value);
    
    // Bold font for key
    QFont font = itemKey->font();
    font.setBold(true);
    itemKey->setFont(font);
    
    if (isError) {
        itemValue->setForeground(Qt::red);
    }
    
    table->setItem(row, 0, itemKey);
    table->setItem(row, 1, itemValue);
    
    // Adjust height to fit content roughly (optional, but helps avoid scrollbars for small tables)
    int height = table->horizontalHeader()->height();
    for (int i = 0; i < table->rowCount(); ++i) height += table->rowHeight(i);
    table->setMinimumHeight(height + 10); // buffer
    table->setMaximumHeight(height + 10);
}

void AboutDialog::collectSystemInfo()
{
    m_sysTable->setRowCount(0);
    m_ltfsTable->setRowCount(0);
    
    // System Info
    addRowToTable(m_sysTable, "OS", getOsInfo());
    addRowToTable(m_sysTable, "Qt Version", qVersion());
    addRowToTable(m_sysTable, "Architecture", QSysInfo::currentCpuArchitecture());
    
    // LTFS Tools
    getLtfsVersion();
    
    // Hardware Info (Fix: Restore this call)
    ui->textHardwareInfo->setPlainText("Scanning hardware...\n");
    ui->textHardwareInfo->setPlainText(getHardwareInfo());
}

QString AboutDialog::getOsInfo()
{
    return QSysInfo::prettyProductName();
}

void AboutDialog::getLtfsVersion()
{
    QString ltfsPath = SettingsManager::instance().ltfsBinaryPath();
    if (ltfsPath.isEmpty()) ltfsPath = "ltfs";
    
    QProcess process;
    process.start(ltfsPath, QStringList() << "--version");
    if (process.waitForFinished(1000)) {
        QString output = process.readAllStandardOutput();
        addRowToTable(m_ltfsTable, "ltfs binary", output.split('\n').first().trimmed());
    } else {
        addRowToTable(m_ltfsTable, "ltfs binary", "Not found", true);
    }
    
    QString mkltfsPath = SettingsManager::instance().mkltfsBinaryPath();
    if (mkltfsPath.isEmpty()) mkltfsPath = "mkltfs";
    
    process.start(mkltfsPath, QStringList() << "--version");
    if (process.waitForFinished(1000)) {
        QString output = process.readAllStandardOutput();
        addRowToTable(m_ltfsTable, "mkltfs binary", output.split('\n').first().trimmed());
    } else {
        addRowToTable(m_ltfsTable, "mkltfs binary", "Not found", true);
    }
    
    QString ltfsckPath = SettingsManager::instance().ltfsckBinaryPath();
    if (ltfsckPath.isEmpty()) ltfsckPath = "ltfsck";
    
    process.start(ltfsckPath, QStringList() << "--version");
    if (process.waitForFinished(1000)) {
        QString output = process.readAllStandardOutput();
        addRowToTable(m_ltfsTable, "ltfsck binary", output.split('\n').first().trimmed());
    } else {
        addRowToTable(m_ltfsTable, "ltfsck binary", "Not found", true);
    }
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
