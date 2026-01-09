#include "DirectRWDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>

DirectRWDialog::DirectRWDialog(DeviceManager* deviceManager, QWidget* parent)
    : QDialog(parent), m_deviceManager(deviceManager)
{
    buildUI();
    refreshDevices();
    applyModeUI();
}

DirectRWDialog::~DirectRWDialog()
{
    if (m_engine) {
        m_engine->stop();
        m_engine->deleteLater();
    }
}

void DirectRWDialog::buildUI()
{
    setWindowTitle("Direct Tape Read/Write");
    resize(720, 560);

    QVBoxLayout* root = new QVBoxLayout(this);

    // Mode selection
    m_radioFileToTape = new QRadioButton("Files → Tape", this);
    m_radioTapeToFile = new QRadioButton("Tape → Folder", this);
    m_radioTapeToTape = new QRadioButton("Tape → Tape", this);
    m_radioFileToTape->setChecked(true);

    connect(m_radioFileToTape, &QRadioButton::toggled, this, &DirectRWDialog::onModeChanged);
    connect(m_radioTapeToFile, &QRadioButton::toggled, this, &DirectRWDialog::onModeChanged);
    connect(m_radioTapeToTape, &QRadioButton::toggled, this, &DirectRWDialog::onModeChanged);

    QHBoxLayout* modeLayout = new QHBoxLayout();
    modeLayout->addWidget(m_radioFileToTape);
    modeLayout->addWidget(m_radioTapeToFile);
    modeLayout->addWidget(m_radioTapeToTape);
    modeLayout->addStretch();
    root->addLayout(modeLayout);

    // Device selectors
    m_sourceDeviceCombo = new QComboBox(this);
    m_destDeviceCombo = new QComboBox(this);
    m_chkRewindSource = new QCheckBox("Rewind source before start", this);
    m_chkRewindDest = new QCheckBox("Rewind destination before start", this);
    m_chkRewindSource->setChecked(true);
    m_chkRewindDest->setChecked(true);

    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow("Source Tape:", m_sourceDeviceCombo);
    formLayout->addRow("Destination Tape:", m_destDeviceCombo);
    formLayout->addRow("Source Options:", m_chkRewindSource);
    formLayout->addRow("Destination Options:", m_chkRewindDest);
    root->addLayout(formLayout);

    // File list section
    m_fileList = new QListWidget(this);
    m_btnAddFiles = new QPushButton("Add Files", this);
    m_btnRemoveFiles = new QPushButton("Remove", this);
    QHBoxLayout* fileBtnLayout = new QHBoxLayout();
    fileBtnLayout->addWidget(m_btnAddFiles);
    fileBtnLayout->addWidget(m_btnRemoveFiles);
    fileBtnLayout->addStretch();

    root->addWidget(new QLabel("Files to Write:", this));
    root->addWidget(m_fileList);
    root->addLayout(fileBtnLayout);

    connect(m_btnAddFiles, &QPushButton::clicked, this, &DirectRWDialog::onBrowseFiles);
    connect(m_btnRemoveFiles, &QPushButton::clicked, this, &DirectRWDialog::onRemoveFiles);

    // Destination folder for tape->file
    m_destFolderEdit = new QLineEdit(this);
    m_btnBrowseFolder = new QPushButton("Browse...", this);
    m_prefixEdit = new QLineEdit("tape_dump", this);
    QHBoxLayout* destLayout = new QHBoxLayout();
    destLayout->addWidget(m_destFolderEdit);
    destLayout->addWidget(m_btnBrowseFolder);
    connect(m_btnBrowseFolder, &QPushButton::clicked, this, &DirectRWDialog::onBrowseFolder);
    root->addWidget(new QLabel("Destination Folder (Tape → Folder):", this));
    root->addLayout(destLayout);
    root->addWidget(new QLabel("Dump Filename Prefix (Tape → Folder):", this));
    root->addWidget(m_prefixEdit);

    // Status and progress
    m_statusLabel = new QLabel("Idle", this);
    m_speedLabel = new QLabel("Speed: 0 MB/s", this);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);

    root->addWidget(m_statusLabel);
    root->addWidget(m_speedLabel);
    root->addWidget(m_progress);
    root->addWidget(new QLabel("Log:", this));
    root->addWidget(m_log, 1);

    // Buttons
    m_btnStart = new QPushButton("Start", this);
    m_btnCancel = new QPushButton("Cancel", this);
    m_btnClose = new QPushButton("Close", this);
    m_btnCancel->setEnabled(false);

    connect(m_btnStart, &QPushButton::clicked, this, &DirectRWDialog::onStart);
    connect(m_btnCancel, &QPushButton::clicked, this, &DirectRWDialog::onCancel);
    connect(m_btnClose, &QPushButton::clicked, this, &DirectRWDialog::reject);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnStart);
    btnLayout->addWidget(m_btnCancel);
    btnLayout->addWidget(m_btnClose);
    root->addLayout(btnLayout);
}

void DirectRWDialog::refreshDevices()
{
    if (!m_deviceManager) return;
    m_devices = m_deviceManager->scanDevices();
    m_sourceDeviceCombo->clear();
    m_destDeviceCombo->clear();
    for (const auto& dev : m_devices) {
        QString label = QString("%1 (%2)").arg(dev.devicePath, dev.productId);
        m_sourceDeviceCombo->addItem(label, dev.devicePath);
        m_destDeviceCombo->addItem(label, dev.devicePath);
    }
}

void DirectRWDialog::onModeChanged()
{
    if (m_radioFileToTape->isChecked()) m_mode = Mode::FileToTape;
    else if (m_radioTapeToFile->isChecked()) m_mode = Mode::TapeToFile;
    else m_mode = Mode::TapeToTape;
    applyModeUI();
}

void DirectRWDialog::applyModeUI()
{
    bool fileMode = (m_mode == Mode::FileToTape);
    bool tapeToFile = (m_mode == Mode::TapeToFile);
    m_fileList->setVisible(fileMode);
    m_btnAddFiles->setVisible(fileMode);
    m_btnRemoveFiles->setVisible(fileMode);
    m_destDeviceCombo->setVisible(fileMode || m_mode == Mode::TapeToTape);
    m_destFolderEdit->setVisible(tapeToFile);
    m_btnBrowseFolder->setVisible(tapeToFile);
    m_prefixEdit->setVisible(tapeToFile);
    m_chkRewindDest->setVisible(fileMode || m_mode == Mode::TapeToTape);
    m_chkRewindSource->setVisible(tapeToFile || m_mode == Mode::TapeToTape);
}

QString DirectRWDialog::selectedSourceDevice() const
{
    return m_sourceDeviceCombo->currentData().toString();
}

QString DirectRWDialog::selectedDestDevice() const
{
    return m_destDeviceCombo->currentData().toString();
}

void DirectRWDialog::onBrowseFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Write");
    for (const QString& f : files) {
        if (!f.isEmpty()) m_fileList->addItem(f);
    }
}

void DirectRWDialog::onRemoveFiles()
{
    qDeleteAll(m_fileList->selectedItems());
}

void DirectRWDialog::onBrowseFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Folder");
    if (!dir.isEmpty()) m_destFolderEdit->setText(dir);
}

bool DirectRWDialog::ensureReady()
{
    if (!m_deviceManager) {
        QMessageBox::warning(this, "Device Manager", "Device manager not available.");
        return false;
    }

    if (m_mode == Mode::FileToTape) {
        if (m_fileList->count() == 0) {
            QMessageBox::warning(this, "Files", "Please add at least one file.");
            return false;
        }
        if (selectedDestDevice().isEmpty()) {
            QMessageBox::warning(this, "Destination", "Please select a destination tape.");
            return false;
        }
    } else if (m_mode == Mode::TapeToFile) {
        if (selectedSourceDevice().isEmpty()) {
            QMessageBox::warning(this, "Source", "Please select a source tape.");
            return false;
        }
        if (m_destFolderEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Destination", "Please choose a destination folder.");
            return false;
        }
        QDir dir(m_destFolderEdit->text());
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this, "Destination", "Cannot create destination folder.");
                return false;
            }
        }
    } else { // TapeToTape
        if (selectedSourceDevice().isEmpty() || selectedDestDevice().isEmpty()) {
            QMessageBox::warning(this, "Devices", "Please select source and destination tapes.");
            return false;
        }
        if (selectedSourceDevice() == selectedDestDevice()) {
            QMessageBox::warning(this, "Devices", "Source and destination must differ.");
            return false;
        }
    }
    return true;
}

void DirectRWDialog::onStart()
{
    if (m_running) return;
    if (!ensureReady()) return;

    if (m_engine) {
        m_engine->stop();
        m_engine->deleteLater();
    }
    m_engine = new TransferEngine(this);
    m_engine->setDeviceManager(m_deviceManager);

    if (m_mode == Mode::FileToTape) {
        QStringList files;
        for (int i = 0; i < m_fileList->count(); ++i) files << m_fileList->item(i)->text();
        m_engine->setSourceFiles(files);
        m_engine->setDestinationDevice(selectedDestDevice());
        m_engine->setRewindDestBefore(m_chkRewindDest->isChecked());
    } else if (m_mode == Mode::TapeToFile) {
        m_engine->setSourceDevice(selectedSourceDevice());
        m_engine->setDestinationPath(m_destFolderEdit->text());
        m_engine->setTapeDumpPrefix(m_prefixEdit->text());
        m_engine->setRewindSourceBefore(m_chkRewindSource->isChecked());
    } else {
        m_engine->setSourceDevice(selectedSourceDevice());
        m_engine->setDestinationDevice(selectedDestDevice());
        m_engine->setRewindSourceBefore(m_chkRewindSource->isChecked());
        m_engine->setRewindDestBefore(m_chkRewindDest->isChecked());
    }

    connect(m_engine, &TransferEngine::progress, this, &DirectRWDialog::onProgress);
    connect(m_engine, &TransferEngine::fileStarted, this, &DirectRWDialog::onFileStarted);
    connect(m_engine, &TransferEngine::fileFinished, this, &DirectRWDialog::onFileFinished);
    connect(m_engine, &TransferEngine::errorOccurred, this, &DirectRWDialog::onError);
    connect(m_engine, &TransferEngine::finished, this, &DirectRWDialog::onFinished);

    m_log->clear();
    m_progress->setValue(0);
    m_statusLabel->setText("Running...");
    m_btnStart->setEnabled(false);
    m_btnCancel->setEnabled(true);
    m_running = true;
    m_engine->start();
}

void DirectRWDialog::onCancel()
{
    if (m_engine) {
        m_engine->stop();
    }
    m_statusLabel->setText("Cancelled");
    m_btnCancel->setEnabled(false);
    m_btnStart->setEnabled(true);
}

void DirectRWDialog::onFinished()
{
    m_running = false;
    m_btnCancel->setEnabled(false);
    m_btnStart->setEnabled(true);
    m_statusLabel->setText("Finished");
}

void DirectRWDialog::onProgress(quint64 bytes, quint64 total, double speed)
{
    if (total > 0) {
        int percent = static_cast<int>((bytes * 100) / total);
        m_progress->setValue(percent);
    }
    m_speedLabel->setText(QString("Speed: %1 MB/s").arg(speed, 0, 'f', 2));
}

void DirectRWDialog::onFileStarted(QString name)
{
    QString msg = QString("[%1] Start: %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), name);
    m_log->append(msg);
    m_statusLabel->setText("Working: " + name);
}

void DirectRWDialog::onFileFinished(QString name, QString checksum)
{
    QString msg = QString("[%1] Done: %2 (SHA1: %3)").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), name, checksum);
    m_log->append(msg);
}

void DirectRWDialog::onError(QString error)
{
    QString msg = QString("[%1] Error: %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), error);
    m_log->append(QString("<font color='red'>%1</font>").arg(msg));
    QMessageBox::critical(this, "Error", error);
    m_running = false;
    m_btnCancel->setEnabled(false);
    m_btnStart->setEnabled(true);
}