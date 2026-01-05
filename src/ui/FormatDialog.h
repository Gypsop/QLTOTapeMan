#ifndef FORMATDIALOG_H
#define FORMATDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include "../device/LtfsManager.h"

class FormatDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FormatDialog(QWidget *parent = nullptr);
    
    LtfsFormatOptions getOptions() const;
    void setVolumeName(const QString &name);
    void setTapeSerial(const QString &serial);

private:
    QLineEdit *m_txtVolumeName;
    QLineEdit *m_txtTapeSerial;
    QComboBox *m_cmbBlockSize;
    QCheckBox *m_chkCompression;
    QCheckBox *m_chkWipe;
    QSpinBox *m_spnIndexPartition;
    QLineEdit *m_txtKeyFile;
    QPushButton *m_btnBrowseKey;
    
    void setupUi();
private slots:
    void onBrowseKey();
};

#endif // FORMATDIALOG_H
