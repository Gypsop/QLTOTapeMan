#include "CheckDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

CheckDialog::CheckDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Check/Recover Tape (ltfsck)");
    setupUi();
}

void CheckDialog::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    layout->addWidget(new QLabel("Select recovery options:", this));
    
    m_chkDeepRecovery = new QCheckBox("Deep Recovery (--deep-recovery)", this);
    m_chkDeepRecovery->setToolTip("Scans the entire tape to recover data. Can take a long time.");
    layout->addWidget(m_chkDeepRecovery);
    
    m_chkFullRecovery = new QCheckBox("Full Recovery (--full-recovery)", this);
    m_chkFullRecovery->setToolTip("Attempts to recover all data blocks, even if index is missing.");
    layout->addWidget(m_chkFullRecovery);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    layout->addWidget(buttonBox);
}

LtfsCheckOptions CheckDialog::getOptions() const
{
    LtfsCheckOptions opts;
    opts.deepRecovery = m_chkDeepRecovery->isChecked();
    opts.fullRecovery = m_chkFullRecovery->isChecked();
    return opts;
}
