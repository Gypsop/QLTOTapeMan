#include "DeviceStatusDialog.h"
#include <QGroupBox>

DeviceStatusDialog::DeviceStatusDialog(const TapeStatus &status, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Device Status");
    resize(400, 500);
    setupUi(status);
}

void DeviceStatusDialog::setupUi(const TapeStatus &status)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Status Group
    QGroupBox *grpStatus = new QGroupBox("General Status", this);
    QFormLayout *formStatus = new QFormLayout(grpStatus);
    
    QLabel *lblState = new QLabel(status.statusMessage, this);
    if (status.isReady) lblState->setStyleSheet("color: green; font-weight: bold;");
    else lblState->setStyleSheet("color: red; font-weight: bold;");
    
    formStatus->addRow("State:", lblState);
    formStatus->addRow("Loaded:", new QLabel(status.isLoaded ? "Yes" : "No", this));
    formStatus->addRow("Write Protected:", new QLabel(status.isWriteProtected ? "Yes" : "No", this));
    formStatus->addRow("Cleaning Required:", new QLabel(status.needsCleaning ? "Yes" : "No", this));
    
    if (status.isLoaded) {
        formStatus->addRow("Current Partition:", new QLabel(QString::number(status.currentPartition), this));
        formStatus->addRow("Current Block:", new QLabel(QString::number(status.currentBlock), this));
    }
    
    mainLayout->addWidget(grpStatus);

    // Capacity Group
    if (status.isLoaded) {
        QGroupBox *grpCapacity = new QGroupBox("Capacity", this);
        QFormLayout *formCapacity = new QFormLayout(grpCapacity);
        
        formCapacity->addRow("Total Capacity:", new QLabel(formatSize(status.capacityBytes), this));
        formCapacity->addRow("Remaining:", new QLabel(formatSize(status.remainingBytes), this));
        
        QProgressBar *prog = new QProgressBar(this);
        if (status.capacityBytes > 0) {
            double used = (double)(status.capacityBytes - status.remainingBytes);
            int percent = (int)((used / status.capacityBytes) * 100.0);
            prog->setValue(percent);
        } else {
            prog->setValue(0);
        }
        formCapacity->addRow("Usage:", prog);
        
        mainLayout->addWidget(grpCapacity);
    }

    // Technical Details
    QGroupBox *grpTech = new QGroupBox("Technical Details", this);
    QFormLayout *formTech = new QFormLayout(grpTech);
    
    formTech->addRow("Block Size:", new QLabel(QString::number(status.blockSize) + " bytes", this));
    formTech->addRow("Max Block Size:", new QLabel(formatSize(status.maxBlockSize), this));
    formTech->addRow("Partition Count:", new QLabel(QString::number(status.partitionCount), this));
    formTech->addRow("Compression:", new QLabel(status.compressionEnabled ? "Enabled" : "Disabled", this));
    
    mainLayout->addWidget(grpTech);
    
    mainLayout->addStretch();
}

QString DeviceStatusDialog::formatSize(uint64_t bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
