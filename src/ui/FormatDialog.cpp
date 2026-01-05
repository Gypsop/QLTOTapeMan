#include "FormatDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>

FormatDialog::FormatDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Format Tape (mkltfs)");
    setupUi();
}

void FormatDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    QGroupBox *grpGeneral = new QGroupBox("General Settings", this);
    QFormLayout *formLayout = new QFormLayout(grpGeneral);
    
    m_txtVolumeName = new QLineEdit(this);
    m_txtVolumeName->setPlaceholderText("TAPE001");
    formLayout->addRow("Volume Name (-n):", m_txtVolumeName);
    
    m_txtTapeSerial = new QLineEdit(this);
    m_txtTapeSerial->setPlaceholderText("Auto-detected or Custom");
    formLayout->addRow("Tape Serial (-s):", m_txtTapeSerial);
    
    mainLayout->addWidget(grpGeneral);
    
    QGroupBox *grpAdvanced = new QGroupBox("Advanced Settings", this);
    QFormLayout *advLayout = new QFormLayout(grpAdvanced);
    
    m_cmbBlockSize = new QComboBox(this);
    m_cmbBlockSize->addItem("Default (512 KB)", 524288);
    m_cmbBlockSize->addItem("256 KB", 262144);
    m_cmbBlockSize->addItem("1 MB", 1048576);
    advLayout->addRow("Block Size (-b):", m_cmbBlockSize);
    
    m_chkCompression = new QCheckBox("Enable Compression (-c)", this);
    m_chkCompression->setChecked(true);
    advLayout->addRow("", m_chkCompression);
    
    m_chkWipe = new QCheckBox("Wipe Tape (-w)", this);
    m_chkWipe->setToolTip("Restore the tape to a non-partitioned state before formatting");
    advLayout->addRow("", m_chkWipe);
    
    m_spnIndexPartition = new QSpinBox(this);
    m_spnIndexPartition->setRange(0, 10000);
    m_spnIndexPartition->setSuffix(" MB");
    m_spnIndexPartition->setSpecialValueText("Default");
    m_spnIndexPartition->setValue(0);
    advLayout->addRow("Index Partition Size:", m_spnIndexPartition);
    
    QHBoxLayout *keyLayout = new QHBoxLayout();
    m_txtKeyFile = new QLineEdit(this);
    m_txtKeyFile->setPlaceholderText("Path to AES key file (Optional)");
    m_btnBrowseKey = new QPushButton("...", this);
    m_btnBrowseKey->setFixedWidth(30);
    connect(m_btnBrowseKey, &QPushButton::clicked, this, &FormatDialog::onBrowseKey);
    keyLayout->addWidget(m_txtKeyFile);
    keyLayout->addWidget(m_btnBrowseKey);
    advLayout->addRow("Encryption Key (-k):", keyLayout);
    
    mainLayout->addWidget(grpAdvanced);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);
}

LtfsFormatOptions FormatDialog::getOptions() const
{
    LtfsFormatOptions opts;
    opts.volumeName = m_txtVolumeName->text();
    opts.tapeSerial = m_txtTapeSerial->text();
    opts.blockSize = m_cmbBlockSize->currentData().toInt();
    opts.compression = m_chkCompression->isChecked();
    opts.wipe = m_chkWipe->isChecked();
    opts.force = true; // Always force if we are here
    opts.indexPartitionSize = m_spnIndexPartition->value();
    opts.keyFile = m_txtKeyFile->text();
    return opts;
}

void FormatDialog::onBrowseKey()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Key File");
    if (!path.isEmpty()) {
        m_txtKeyFile->setText(path);
    }
}

void FormatDialog::setVolumeName(const QString &name)
{
    m_txtVolumeName->setText(name);
}

void FormatDialog::setTapeSerial(const QString &serial)
{
    m_txtTapeSerial->setText(serial);
}
