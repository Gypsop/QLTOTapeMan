#include "TransferDialog.h"
#include <QMessageBox>

TransferDialog::TransferDialog(const QStringList& files, const QString& dest, DeviceManager* deviceManager, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("File Transfer");
    resize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("Preparing...", this);
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    layout->addWidget(m_progressBar);
    
    m_speedLabel = new QLabel("Speed: 0 MB/s", this);
    layout->addWidget(m_speedLabel);

    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    layout->addWidget(m_logView);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_cancelButton = new QPushButton("Cancel", this);
    m_closeButton = new QPushButton("Close", this);
    m_closeButton->setEnabled(false);
    
    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelButton);
    btnLayout->addWidget(m_closeButton);
    layout->addLayout(btnLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, &TransferDialog::onCancel);
    connect(m_closeButton, &QPushButton::clicked, this, &TransferDialog::accept);

    // Setup Engine
    m_engine = new TransferEngine(this);
    m_engine->setSourceFiles(files);
    
    if (deviceManager) {
        m_engine->setDeviceManager(deviceManager);
        m_engine->setDestinationDevice(dest);
        setWindowTitle("Writing to Tape (" + dest + ")");
    } else {
        m_engine->setDestinationPath(dest);
    }

    connect(m_engine, &TransferEngine::progress, this, &TransferDialog::onProgress);
    connect(m_engine, &TransferEngine::fileStarted, this, &TransferDialog::onFileStarted);
    connect(m_engine, &TransferEngine::fileFinished, this, &TransferDialog::onFileFinished);
    connect(m_engine, &TransferEngine::errorOccurred, this, &TransferDialog::onError);
    connect(m_engine, &TransferEngine::finished, this, &TransferDialog::onFinished);

    // Start automatically
    m_engine->start();
}

TransferDialog::~TransferDialog()
{
    if (m_engine->isRunning()) {
        m_engine->stop();
    }
}

void TransferDialog::onProgress(quint64 bytes, quint64 total, double speed)
{
    if (total > 0) {
        int percent = (int)((bytes * 100) / total);
        m_progressBar->setValue(percent);
    }
    m_speedLabel->setText(QString("Speed: %1 MB/s").arg(speed, 0, 'f', 2));
}

void TransferDialog::onFileStarted(QString name)
{
    m_statusLabel->setText("Copying: " + name);
    m_logView->append("Started: " + name);
}

void TransferDialog::onFileFinished(QString name, QString checksum)
{
    m_logView->append(QString("Finished: %1 (SHA1: %2)").arg(name, checksum));
}

void TransferDialog::onError(QString error)
{
    m_logView->append("<font color='red'>Error: " + error + "</font>");
    QMessageBox::critical(this, "Transfer Error", error);
}

void TransferDialog::onFinished()
{
    m_statusLabel->setText("Transfer Completed");
    m_progressBar->setValue(100);
    m_cancelButton->setEnabled(false);
    m_closeButton->setEnabled(true);
    m_logView->append("<b>All tasks finished.</b>");
}

void TransferDialog::onCancel()
{
    if (QMessageBox::question(this, "Cancel", "Are you sure you want to cancel the transfer?") == QMessageBox::Yes) {
        m_engine->stop();
        m_statusLabel->setText("Cancelled");
        m_logView->append("<font color='orange'>Transfer cancelled by user.</font>");
        m_cancelButton->setEnabled(false);
        m_closeButton->setEnabled(true);
    }
}
